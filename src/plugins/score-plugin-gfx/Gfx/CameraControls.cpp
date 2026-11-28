#include "CameraControls.hpp"

#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>
#include <ossia/network/domain/domain.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QDebug>

#include <future>

namespace Gfx
{
CameraControlBackend::~CameraControlBackend() = default;

CameraControls::CameraControls(std::unique_ptr<CameraControlBackend> backend, bool threaded)
    : m_backend{std::move(backend)}
{
  if(!threaded)
  {
    load();
    return;
  }

  auto ready = std::make_shared<std::promise<void>>();
  auto opened = ready->get_future();
  m_thread = std::thread{[this, ready] {
    load();
    ready->set_value();
    run();
  }};
  opened.wait();
}

CameraControls::~CameraControls()
{
  stop();
}

void CameraControls::load()
{
  auto controls = m_backend->open();
  for(std::size_t i = 0; i < controls.size(); i++)
  {
    auto state = m_backend->get(i);
    if(!state || !(controls[i].min <= controls[i].max))
      continue;
    Control c;
    c.index = i;
    c.info = std::move(controls[i]);
    c.state = *state;
    m_controls.push_back(std::move(c));
  }
}

void CameraControls::setup(ossia::net::node_base& root)
{
  for(auto& c : m_controls)
  {
    const std::string base = CameraControlNames::root;
    auto& node = ossia::net::create_node(root, base + c.info.name);
    c.param = node.create_parameter(
        c.info.is_float ? ossia::val_type::FLOAT : ossia::val_type::INT);
    if(c.param)
    {
      if(c.info.is_float)
        c.param->set_domain(ossia::make_domain(float(c.info.min), float(c.info.max)));
      else
        c.param->set_domain(
            ossia::make_domain(int(std::lround(c.info.min)), int(std::lround(c.info.max))));
    }

    if(!c.info.auto_name.empty())
    {
      auto& auto_node = ossia::net::create_node(root, base + c.info.auto_name);
      c.auto_param = auto_node.create_parameter(ossia::val_type::BOOL);
    }
    publish(c);
  }
  m_published = true;
}

bool CameraControls::push(const ossia::net::parameter_base& param, const ossia::value& v)
{
  for(auto& c : m_controls)
  {
    if(&param != c.param && &param != c.auto_param)
      continue;

    std::lock_guard lock{m_mutex};
    if(!m_running)
      return false;
    if(&param == c.param)
    {
      const double x = ossia::convert<double>(v);
      if(!std::isfinite(x))
        return false;
      c.pending.value = true;
      c.pending.v = x;
      c.pending.mode = true;
      c.pending.automatic = false;
    }
    else
    {
      c.pending.mode = true;
      c.pending.automatic = ossia::convert<bool>(v);
      if(c.pending.automatic)
        c.pending.value = false;
    }
    m_dirty = true;
    m_cv.notify_one();
    return true;
  }
  return false;
}

void CameraControls::streamStarted()
{
  std::lock_guard lock{m_mutex};
  const auto now = clock::now();
  m_enforce_until = now + enforce_duration;
  m_next_poll = now;
  m_cv.notify_one();
}

void CameraControls::stop()
{
  {
    std::lock_guard lock{m_mutex};
    m_running = false;
  }
  m_cv.notify_all();
  if(m_thread.joinable())
    m_thread.join();
  if(!m_closed)
  {
    m_backend->close();
    m_closed = true;
  }
}

void CameraControls::run()
{
  for(;;)
  {
    {
      std::unique_lock lock{m_mutex};
      m_cv.wait_until(lock, m_next_poll, [this] { return !m_running || m_dirty; });
      if(!m_running)
        break;
    }
    step(clock::now());
  }
  m_backend->close();
  m_closed = true;
}

void CameraControls::step(clock::time_point now)
{
  thread_local std::vector<std::pair<Control*, Pending>> writes;
  writes.clear();
  bool enforce{}, poll{};
  {
    std::lock_guard lock{m_mutex};
    if(m_dirty)
    {
      for(auto& c : m_controls)
      {
        if(c.pending.value || c.pending.mode)
          writes.emplace_back(&c, c.pending);
        c.pending = {};
      }
      m_dirty = false;
    }
    enforce = now < m_enforce_until;
    poll = now >= m_next_poll;
    if(poll)
      m_next_poll = now + (enforce ? enforce_interval : poll_interval);
  }

  for(auto& [c, p] : writes)
    write(*c, p);
  if(poll && !m_lost && m_published)
    refresh(enforce);
}

void CameraControls::write(Control& c, const Pending& p)
{
  if(!p.value)
    if(auto s = m_backend->get(c.index))
      c.state = *s;
  const double value = p.value ? p.v : c.state.value;
  const bool automatic = p.mode && p.automatic;
  const bool ok = m_backend->set(c.index, value, automatic);
  if(auto s = m_backend->get(c.index))
  {
    c.state = *s;
    publish(c);
  }
  if(ok)
    c.wanted = {true, value, automatic, c.state.value};
  else
    qDebug() << "Camera control" << c.info.name.c_str() << ": could not set" << value;
}

bool CameraControls::needsReapply(
    const Wanted& w, CameraControlBackend::State read, bool hasAuto) noexcept
{
  if(!w.written)
    return false;
  if(hasAuto && read.automatic != w.automatic)
    return true;
  return !w.automatic && read.value != w.applied;
}

void CameraControls::refresh(bool enforce)
{
  int reads = 0, failures = 0;
  for(auto& c : m_controls)
  {
    const bool hasAuto = !c.info.auto_name.empty();
    if(!(c.state.automatic && hasAuto) && !(enforce && c.wanted.written))
      continue;

    reads++;
    const auto s = m_backend->get(c.index);
    if(!s)
    {
      failures++;
      continue;
    }

    if(enforce && needsReapply(c.wanted, *s, hasAuto))
    {
      qDebug() << "Camera control" << c.info.name.c_str() << ": reset to" << s->value
               << "by the device, set again";
      if(!m_backend->set(c.index, c.wanted.value, c.wanted.automatic))
        c.wanted.written = false;
      if(auto r = m_backend->get(c.index))
      {
        c.state = *r;
        publish(c);
      }
    }
    else if(s->value != c.state.value || s->automatic != c.state.automatic)
    {
      c.state = *s;
      publish(c);
    }
  }

  if(reads > 0 && failures == reads)
  {
    if(++m_failedPolls == 3)
    {
      m_lost = true;
      qDebug() << "Camera controls: the device no longer answers, not polled any more";
    }
  }
  else
    m_failedPolls = 0;
}

void CameraControls::publish(Control& c)
{
  if(c.param)
  {
    if(c.info.is_float)
      c.param->set_value(float(c.state.value));
    else
      c.param->set_value(int(std::lround(c.state.value)));
  }
  if(c.auto_param)
    c.auto_param->set_value(c.state.automatic);
}

#if !defined(_WIN32)
std::shared_ptr<CameraControls> make_camera_controls(const CameraSettings&)
{
  return {};
}
#endif
}
