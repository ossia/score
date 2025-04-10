#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Engine/ApplicationPlugin.hpp>

#include <score/tools/IdentifierGeneration.hpp>

#include <ossia/network/domain/domain_functions.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <RemoteControl/Controller/DocumentPlugin.hpp>
#include <RemoteControl/Controller/RemoteControlProvider.hpp>

namespace RemoteControl::Controller
{

DocumentPlugin::DocumentPlugin(const score::DocumentContext& doc, QObject* parent)
    : score::DocumentPlugin{doc, "RemoteControl::Controller::DocumentPlugin", parent}
{
  connect(
      &doc.selectionStack, &score::SelectionStack::currentSelectionChanged, this,
      &DocumentPlugin::on_selectionChanged);
}

DocumentPlugin::~DocumentPlugin() { }

void DocumentPlugin::on_selectionChanged(const Selection& old, const Selection& current)
{
  deselectProcess();

  if(current.size() == 1)
  {
    if(auto p = qobject_cast<Process::ProcessModel*>(current.at(0)))
    {
      if(p->flags() & Process::ProcessFlags::ControlSurface)
      {
        selectProcess(*p);
        return;
      }
    }
  }
}

void DocumentPlugin::deselectProcess()
{
  m_currentProcess = nullptr;
  m_currentControls.clear();
  m_displayedControls = {};
  m_current_bank_offset = 0;
  m_current_channel_offset = 0;

  // Disconnect connections
  for(auto& [p, v] : m_currentConnections)
  {
    for(QMetaObject::Connection& con : v)
    {
      QObject::disconnect(con);
    }
  }
  m_currentConnections.clear();

  clearDisplay();
}

void DocumentPlugin::clearDisplay()
{
  // Clear the remote controls watching it
  for(auto& rc : this->m_controllers)
  {
    for(auto& map : rc.maps)
    {
      rc.controller->controlNameChanged(map.handle, {});
      rc.controller->controlValueChanged(map.handle, {});
      map.inlet = {};
    }
  }
}

void DocumentPlugin::on_controlChanged(
    Process::ProcessModel& p, Process::ControlInlet& inl, int idx, const ossia::value& v)
{
  idx -= (8 * m_current_bank_offset + m_current_channel_offset);
  for(auto& ctl : this->m_controllers)
  {
    int n = ctl.maps.size();
    if(n <= idx)
    {
      idx -= n;
      continue;
    }
    auto handle = ctl.maps[idx].handle;
    ctl.controller->controlValueChanged(handle, v);
  }
}

void DocumentPlugin::selectProcess(Process::ProcessModel& p)
{
  m_currentControls.clear();
  m_displayedControls = {};
  m_currentProcess = &p;
  m_current_bank_offset = 0;
  m_current_channel_offset = 0;
  auto& vec = m_currentConnections[&p];

  // 1. Connect the controls of the process
  for(auto& inl : p.inlets())
  {
    if(auto ctl = qobject_cast<Process::ControlInlet*>(inl))
    {
      auto t = ctl->value().get_type();
      if(t != ossia::val_type::BOOL && t != ossia::val_type::INT
         && t != ossia::val_type::FLOAT)
        continue;
      int idx = m_currentControls.size();
      m_currentControls.push_back(ctl);
      // Setup the link
      auto con = connect(
          ctl, &Process::ControlInlet::valueChanged, this,
          [this, &p, ctl, idx](const ossia::value& v) {
        on_controlChanged(p, *ctl, idx, v);
      }, Qt::QueuedConnection);
      vec.push_back(con);
    }
  }

  m_displayedControls = m_currentControls;

  // 2. Update whenever the process changes
  {
    vec.push_back(connect(
        &p, &Process::ProcessModel::inletsChanged, this, &DocumentPlugin::reloadProcess,
        Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection)));
    vec.push_back(connect(
        &p, &Process::ProcessModel::controlAdded, this, &DocumentPlugin::reloadProcess,
        Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection)));
    vec.push_back(connect(
        &p, &Process::ProcessModel::controlRemoved, this, &DocumentPlugin::reloadProcess,
        Qt::ConnectionType(Qt::QueuedConnection | Qt::UniqueConnection)));

    vec.push_back(
        connect(&p, &Process::ProcessModel::identified_object_destroying, this, [this] {
      this->deselectProcess();
    }));
  }

  // 3. Update the display
  updateDisplay();
}

void DocumentPlugin::reloadProcess()
{
  auto proc = m_currentProcess;
  this->deselectProcess();
  if(proc)
    this->selectProcess(*proc);
}

void DocumentPlugin::updateDisplayedControls()
{
  m_displayedControls = m_currentControls;
  const int N = std::ssize(m_displayedControls);
  const int offset = 8 * m_current_bank_offset + m_current_channel_offset;

  if(offset >= 0 && N < offset)
    return;
  m_displayedControls = m_displayedControls.subspan(offset);
}

void DocumentPlugin::connectToEngine()
{
  if(m_controllers.empty())
    return;

  if(!m_engine)
  {
    m_engine = m_context.app.findGuiApplicationPlugin<Engine::ApplicationPlugin>();
  }

  if(m_engine)
  {
    connect(
        &m_engine->execution_ui_clock_timer, &QTimer::timeout, this,
        &DocumentPlugin::on_execTime, Qt::UniqueConnection);
  }
}

void DocumentPlugin::disconnectFromEngine()
{
  if(m_engine)
  {
    disconnect(
        &m_engine->execution_ui_clock_timer, &QTimer::timeout, this,
        &DocumentPlugin::on_execTime);
  }
}

void DocumentPlugin::updateDisplay()
{
  clearDisplay();
  updateDisplayedControls();
  int controller_idx = 0;
  int controller_control_idx = 0;
  for(auto ctl : m_displayedControls)
  {
    if(controller_idx < m_controllers.size())
    {
      auto& rc = m_controllers[controller_idx];
      if(controller_control_idx < rc.maps.size())
      {
        auto& maps = rc.maps[controller_control_idx];
        auto handle = maps.handle;
        maps.inlet = ctl;
        rc.controller->controlNameChanged(handle, ctl->name());
        rc.controller->controlValueChanged(handle, ctl->value());
        controller_control_idx++;
      }
      else
      {
        do
        {
          controller_idx++;
          controller_control_idx = 0;
        } while(controller_idx < m_controllers.size()
                && m_controllers[controller_idx].maps.empty());
      }
    }
    else
    {
      break;
    }
  }
}

void DocumentPlugin::prevBank(RemoteControlImpl& b)
{
  if(m_currentProcess)
  {
    if(m_current_bank_offset == 0)
      return;

    if(m_current_channel_offset > 0)
      m_current_channel_offset = 0;
    else
      m_current_bank_offset--;
  }
  else
  {
    m_current_bank_offset = 0;
    m_current_channel_offset = 0;
  }
  updateDisplay();
}

void DocumentPlugin::nextBank(RemoteControlImpl& b)
{
  if(m_currentProcess)
  {
    // if we don't have at least one thing to show, do nothing
    if((m_current_bank_offset + 1) * 8 >= std::ssize(m_currentControls))
      return;

    m_current_bank_offset++;
  }
  else
  {
    m_current_bank_offset = 0;
  }
  m_current_channel_offset = 0;
  updateDisplay();
}

void DocumentPlugin::prevChannel(RemoteControlImpl& b)
{
  if(m_currentProcess)
  {
    if(m_current_channel_offset == 0)
    {
      if(m_current_bank_offset > 0)
      {
        m_current_bank_offset--;
        m_current_channel_offset = 7;
      }
    }
    else
    {
      m_current_channel_offset--;
    }
  }
  else
  {
    m_current_bank_offset = 0;
    m_current_channel_offset = 0;
  }
  updateDisplay();
}

void DocumentPlugin::nextChannel(RemoteControlImpl& b)
{
  if(m_currentProcess)
  {
    // if we don't have at least one thing to show, do nothing
    if(m_current_bank_offset * 8 + m_current_channel_offset + 1
       >= std::ssize(m_currentControls))
      return;

    m_current_channel_offset++;
    if(m_current_channel_offset == 8)
    {
      m_current_bank_offset++;
      m_current_channel_offset = 0;
    }
  }
  else
  {
    m_current_bank_offset = 0;
    m_current_channel_offset = 0;
  }
  updateDisplay();
}

std::shared_ptr<Process::RemoteControlInterface> DocumentPlugin::acquireRemoteControlInterface()
{
  auto impl = std::make_shared<RemoteControlImpl>(*this);
  m_controllers.push_back({impl});
  connectToEngine();

  return impl;
}

void DocumentPlugin::releaseRemoteControlInterface(
    std::shared_ptr<Process::RemoteControlInterface> impl)
{
  ossia::remove_erase_if(
      m_controllers, [&impl](const Controller& e) { return e.controller == impl; });

  if(m_controllers.empty())
  {
    disconnectFromEngine();
  }
}

template <typename Vector>
static auto getIdRange(std::size_t s, const Vector& existing)
{
  auto existing_size = existing.size();
  auto total_size = existing_size + s;
  Vector vec;
  vec.reserve(total_size);

  // Copy the existing ids
  vec.insert(vec.begin(), existing.begin(), existing.end());

  // Then generate the new ones
  for(std::size_t i = 0; i < s; i++)
    vec.push_back(score::id_generator::getNextId(vec));

  return Vector(vec.begin() + existing.size(), vec.end());
}

void DocumentPlugin::setControl(
    RemoteControlImpl& c, ControllerHandle index, const ossia::value& val)
{
  for(auto& ctls : m_controllers)
  {
    if(ctls.controller.get() == &c)
    {
      for(auto& [h, inl] : ctls.maps)
      {
        if(h == index && inl)
        {
          inl->setValue(val);
          break;
        }
      }
      break;
    }
  }
}

void DocumentPlugin::offsetControl(
    RemoteControlImpl& c, ControllerHandle index, double offset)
{
  for(auto& ctls : m_controllers)
  {
    if(ctls.controller.get() == &c)
    {
      for(auto& [h, inl] : ctls.maps)
      {
        if(h == index && inl)
        {
          auto orig = inl->value();

          auto dmin = ossia::convert<double>(ossia::get_min(inl->domain().get()));
          auto dmax = ossia::convert<double>(ossia::get_max(inl->domain().get()));
          if(dmin >= dmax)
          {
            dmin = 0.;
            dmax = 1.;
          }
          auto offset_number = [&](double f) {
            double v = (f - dmin) / (dmax - dmin);
            v += 0.01f * offset;
            v = std::clamp(v, 0., 1.);
            return v * (dmax - dmin) + dmin;
          };
          switch(orig.get_type())
          {
            case ossia::val_type::INT: {
              inl->setValue(offset_number(*orig.target<int>()));
              break;
            }
            case ossia::val_type::FLOAT: {
              inl->setValue(offset_number(*orig.target<float>()));
              break;
            }
            case ossia::val_type::VEC2F: {
              auto v = *orig.target<ossia::vec2f>();
              v[0] = offset_number(v[0]);
              v[1] = offset_number(v[1]);
              inl->setValue(v);
              break;
            }
            case ossia::val_type::VEC3F: {
              auto v = *orig.target<ossia::vec3f>();
              v[0] = offset_number(v[0]);
              v[1] = offset_number(v[1]);
              v[2] = offset_number(v[2]);
              inl->setValue(v);
              break;
            }
            case ossia::val_type::VEC4F: {
              auto v = *orig.target<ossia::vec4f>();
              v[0] = offset_number(v[0]);
              v[1] = offset_number(v[1]);
              v[2] = offset_number(v[2]);
              v[3] = offset_number(v[3]);
              inl->setValue(v);
              break;
            }
            case ossia::val_type::BOOL: {
              inl->setValue(bool(offset >= 0));
              break;
            }

              // FIXME a bit rough but this case is pretty much inexisting anyways
            case ossia::val_type::LIST: {
              auto& v = *orig.target<std::vector<ossia::value>>();
              for(ossia::value& e : v)
                e = (float)offset_number(ossia::convert<float>(e));
              inl->setValue(std::move(v));
              break;
            }
            case ossia::val_type::MAP: {
              auto& v = *orig.target<ossia::value_map_type>();
              for(auto& [k, e] : v)
                e = (float)offset_number(ossia::convert<float>(e));
              inl->setValue(std::move(v));
              break;
            }
            case ossia::val_type::STRING:
            case ossia::val_type::IMPULSE:
            case ossia::val_type::NONE:
              break;
          }
          break;
        }
      }
      break;
    }
  }
}

void DocumentPlugin::on_execTime()
{
  if(m_controllers.empty())
    return;

  auto t = m_engine->execution().execution_time();

  for(auto& ctl : this->m_controllers)
  {
    ctl.controller->transportChanged(t, 1., 1., 1., 1.);
  }
}
std::vector<Process::RemoteControlInterface::ControllerHandle>
DocumentPlugin::registerControllerGroup(
    RemoteControlImpl& c, Process::RemoteControlInterface::ControllerHint hint, int count)
{
  for(auto& ctls : m_controllers)
  {
    if(ctls.controller.get() == &c)
    {
      auto range = getIdRange(count, ctls.handles);
      ctls.handles.insert(ctls.handles.end(), range.begin(), range.end());
      if(hint & Process::RemoteControlInterface::ControllerHint::MapControls)
      {
        for(auto e : range)
        {
          ctls.maps.push_back(Controller::ControlMap{e, nullptr});
        }
      }

      return range;
    }
  }
  return {};
}
}
