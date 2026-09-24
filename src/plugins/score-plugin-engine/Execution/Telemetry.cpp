#include "Telemetry.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <Audio/Settings/Model.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/Settings/ExecutorModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

#include <QTimer>

#include <algorithm>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Execution::Telemetry)

namespace Execution
{
using ossia::telemetry::tap_kind;

Telemetry::Telemetry(const score::DocumentContext& ctx, DocumentPlugin& plug)
    : m_context{ctx}
    , m_plugin{plug}
{
  auto& scenar = ctx.app.settings<Scenario::Settings::Model>();
  con(scenar, &Scenario::Settings::Model::ExecutionUpdateChanged, this, [this](bool) {
    if(!m_running)
      return;
    if(enabled())
      rebuild();
    else
      teardown();
  });
  con(ctx.coarseUpdateTimer, &QTimer::timeout, this, &Telemetry::read);
}

Telemetry::~Telemetry()
{
  for(auto& s : m_subs)
    if(s.param)
      s.param->meter.store(nullptr, std::memory_order_release);
}

bool Telemetry::enabled() const noexcept
{
  return m_context.app.settings<Scenario::Settings::Model>().getExecutionUpdate();
}

uint64_t Telemetry::publishInterval() const noexcept
{
  const auto rate = m_context.app.settings<Audio::Settings::Model>().getRate();
  const auto ms = std::max(1, m_context.coarseUpdateTimer.interval());
  return std::max<uint64_t>(1, uint64_t(rate) * ms / 1000);
}

Telemetry::Meter Telemetry::subscribe(
    tap_kind kind, const Process::Port* port, bool inlet,
    ossia::virtual_audio_parameter* param)
{
  for(std::size_t i = 0; i < m_subs.size(); i++)
  {
    auto& s = m_subs[i];
    if(s.users > 0 && s.kind == kind && s.port == port && s.inlet == inlet
       && s.param == param)
    {
      s.users++;
      return {int(i), s.generation};
    }
  }

  int index{};
  if(!m_free.empty())
  {
    index = m_free.back();
    m_free.pop_back();
  }
  else
  {
    index = int(m_subs.size());
    m_subs.emplace_back();
  }

  static uint32_t generations = 0;
  auto& s = m_subs[index];
  s = Subscription{};
  s.kind = kind;
  s.port = port;
  s.inlet = inlet;
  s.generation = ++generations;
  s.users = 1;
  s.param = param;

  if(m_arena)
  {
    if(std::size_t(index) < m_arena->meter_capacity())
      attach(index);
    else
      rebuild();
  }
  return {index, s.generation};
}

Telemetry::Meter Telemetry::meterOutlet(const Process::AudioOutlet& outlet)
{
  return subscribe(tap_kind::node, &outlet, false);
}

Telemetry::Meter Telemetry::meterInlet(const Process::AudioInlet& inlet)
{
  return subscribe(tap_kind::node, &inlet, true);
}

Telemetry::Meter Telemetry::meterHardwareInputs()
{
  return subscribe(tap_kind::hardware_inputs, nullptr, false);
}

Telemetry::Meter Telemetry::meterHardwareOutputs()
{
  return subscribe(tap_kind::hardware_outputs, nullptr, false);
}

Telemetry::Meter Telemetry::meterVirtualPort(ossia::virtual_audio_parameter& port)
{
  return subscribe(tap_kind::node, nullptr, false, &port);
}

void Telemetry::forgetUnder(const ossia::net::node_base& root)
{
  // Called while the nodes and their parameters still exist.
  for(std::size_t i = 0; i < m_subs.size(); i++)
  {
    auto& s = m_subs[i];
    if(!s.param)
      continue;
    for(auto n = &s.param->get_node(); n; n = n->get_parent())
    {
      if(n == &root)
      {
        detach(int(i));
        s.param = nullptr;
        break;
      }
    }
  }
}

void Telemetry::release(Meter m)
{
  if(!m || std::size_t(m.index) >= m_subs.size())
    return;
  auto& s = m_subs[m.index];
  if(s.generation != m.generation || s.users == 0)
    return;
  if(--s.users > 0)
    return;

  detach(m.index);
  s = Subscription{};
  m_free.push_back(m.index);
}

const ossia::telemetry::meter_levels* Telemetry::levels(Meter m) const noexcept
{
  if(!m_arena || !m || std::size_t(m.index) >= m_subs.size())
    return nullptr;
  const auto& sub = m_subs[m.index];
  if(sub.users == 0 || sub.generation != m.generation)
    return nullptr;
  const auto& meters = m_arena->latest().meters;
  if(std::size_t(m.index) >= meters.size())
    return nullptr;
  const auto& slot = meters[m.index];
  if(slot.generation != m.generation || slot.levels.ticks == 0)
    return nullptr;
  return &slot.levels;
}

int Telemetry::sampleRate() const noexcept
{
  return m_arena ? m_arena->latest().sample_rate : 0;
}

Telemetry::Playhead
Telemetry::registerPlayhead(std::shared_ptr<ossia::telemetry::playhead_tap> tap)
{
  int index{};
  if(!m_freePlayheads.empty())
  {
    index = m_freePlayheads.back();
    m_freePlayheads.pop_back();
  }
  else
  {
    index = int(m_playheads.size());
    m_playheads.emplace_back();
  }

  static uint32_t generations = 0;
  auto& p = m_playheads[index];
  p = PlayheadSub{std::move(tap), ++generations, false};

  if(m_arena)
  {
    if(std::size_t(index) < m_arena->playhead_capacity())
      attachPlayhead(index);
    else
      rebuild();
  }
  return {index, p.generation};
}

void Telemetry::attachPlayhead(int index)
{
  auto& ctx = m_plugin.contextData();
  auto& p = m_playheads[index];
  if(!ctx || !m_arena || p.attached || !p.tap)
    return;
  ctx->m_execQueue.enqueue(
      [arena = m_arena, index, gen = p.generation, tap = p.tap]() mutable {
    arena->attach_playhead(index, gen, tap);
  });
  p.attached = true;
}

void Telemetry::release(Playhead h)
{
  if(!h || std::size_t(h.index) >= m_playheads.size())
    return;
  auto& p = m_playheads[h.index];
  if(p.generation != h.generation || !p.tap)
    return;

  auto& ctx = m_plugin.contextData();
  if(ctx && m_arena && p.attached)
  {
    ctx->m_execQueue.enqueue(
        [arena = m_arena, index = h.index,
         tap = std::shared_ptr<ossia::telemetry::playhead_tap>{}]() mutable {
      arena->attach_playhead(index, 0, tap);
    });
  }
  p = PlayheadSub{};
  m_freePlayheads.push_back(h.index);
}

const ossia::telemetry::playhead_slot* Telemetry::playhead(Playhead h) const noexcept
{
  if(!m_arena || !h || std::size_t(h.index) >= m_playheads.size())
    return nullptr;
  if(m_playheads[h.index].generation != h.generation)
    return nullptr;
  const auto& slots = m_arena->latest().playheads;
  if(std::size_t(h.index) >= slots.size() || slots[h.index].generation != h.generation)
    return nullptr;
  return &slots[h.index];
}

double Telemetry::cpuLoad(const Process::ProcessModel& proc) const noexcept
{
  if(!m_arena)
    return -1.;
  auto it = m_benchOfProcess.find(&proc);
  if(it == m_benchOfProcess.end())
    return -1.;
  const auto& f = m_arena->latest();
  const auto i = std::size_t(it->second);
  if(i >= f.benches.size() || f.benches[i].generation != m_benches[i].generation)
    return -1.;
  return f.load(f.benches[i]);
}

bool Telemetry::benchEnabled() const noexcept
{
  auto& ctx = m_plugin.contextData();
  return ctx && ctx->bench && m_plugin.settings.getBench();
}

bool Telemetry::syncBenches()
{
  auto& ctx = m_plugin.contextData();
  if(!m_arena || !benchEnabled())
    return true;

  const auto& procs = ctx->setupContext.proc_map;

  // Nodes that left the graph give their slot back. The node itself is gone
  // or going: only the arena lets go of the tap.
  for(std::size_t i = 0; i < m_benches.size(); i++)
  {
    auto& b = m_benches[i];
    if(!b.node)
      continue;
    auto it = procs.find(b.node);
    if(it != procs.end() && it->second == b.process)
      continue;

    ctx->m_execQueue.enqueue(
        [arena = m_arena, i, tap = std::shared_ptr<ossia::telemetry::bench_tap>{}]() mutable {
      arena->attach_bench(i, 0, tap);
    });
    m_benchOfNode.erase(b.node);
    if(auto p = m_benchOfProcess.find(b.process.data());
       p != m_benchOfProcess.end() && p->second == int(i))
      m_benchOfProcess.erase(p);
    b = Bench{};
    m_freeBenches.push_back(int(i));
  }

  // New nodes get a slot and a tap. A node still in proc_map has not been
  // removed yet, and its removal will be queued after this.
  static uint32_t generations = 0;
  for(const auto& [node, proc] : procs)
  {
    if(!node || !proc)
      continue;
    if(m_benchOfNode.contains(node))
      continue;

    int index{};
    if(!m_freeBenches.empty())
    {
      index = m_freeBenches.back();
      m_freeBenches.pop_back();
    }
    else
    {
      index = int(m_benches.size());
      m_benches.emplace_back();
    }
    if(std::size_t(index) >= m_arena->bench_capacity())
      return false;

    auto& b = m_benches[index];
    b.node = node;
    b.process = proc;
    b.generation = ++generations;
    m_benchOfNode[node] = index;
    m_benchOfProcess[proc] = index;

    auto tap = std::make_shared<ossia::telemetry::bench_tap>();
    ctx->m_execQueue.enqueue(
        [arena = m_arena, index, gen = b.generation, in_arena = tap, in_node = tap,
         n = const_cast<ossia::graph_node*>(node)]() mutable {
      arena->attach_bench(index, gen, in_arena);
      std::swap(n->bench_tap, in_node);
    });
  }
  return true;
}

void Telemetry::reportBenches()
{
  if(!m_arena)
    return;
  const auto& f = m_arena->latest();
  for(std::size_t i = 0; i < m_benches.size() && i < f.benches.size(); i++)
  {
    const auto& b = m_benches[i];
    if(b.process && f.benches[i].generation == b.generation)
      const_cast<Process::ProcessModel*>(b.process.data())
          ->benchmark(100. * f.load(f.benches[i]));
  }
}

void Telemetry::executionStarted()
{
  m_running = true;
  if(enabled())
    rebuild();
}

void Telemetry::executionStopped()
{
  // The graph goes away with its outlets and its context: nothing to detach.
  // The virtual ports stay, and must forget the taps before the arena goes.
  for(auto& s : m_subs)
    if(s.param)
      s.param->meter.store(nullptr, std::memory_order_release);
  m_running = false;
  m_arena.reset();
  m_benches.clear();
  m_freeBenches.clear();
  m_benchOfNode.clear();
  m_benchOfProcess.clear();
  for(auto& p : m_playheads)
    p.attached = false;
  for(auto& s : m_subs)
  {
    s.attached = false;
    s.node.reset();
    s.ossia_inlet = nullptr;
    s.ossia_outlet = nullptr;
  }
  updated();
}

void Telemetry::rebuild()
{
  auto& ctx = m_plugin.contextData();
  if(!ctx)
    return;

  // The views keep what they show until the new arena's first frame.
  teardown(false);

  const std::size_t capacity = std::max<std::size_t>(64, 2 * m_subs.size());
  const std::size_t benches
      = benchEnabled()
            ? std::max<std::size_t>(
                  64, 2 * std::max(m_benches.size(), ctx->setupContext.proc_map.size()))
            : 0;
  const std::size_t playheads = std::max<std::size_t>(256, 2 * m_playheads.size());
  m_arena = std::make_shared<ossia::telemetry::arena>(capacity, benches, playheads);
  m_arena->set_publish_interval(publishInterval());

  ctx->m_execQueue.enqueue(
      [c = std::weak_ptr{ctx}, arena = m_arena]() mutable {
    // The previous arena, if any, leaves with this closure.
    if(auto ctx = c.lock())
      std::swap(ctx->telemetry, arena);
  });

  for(std::size_t i = 0; i < m_subs.size(); i++)
    if(m_subs[i].users > 0)
      attach(i);

  for(std::size_t i = 0; i < m_playheads.size(); i++)
  {
    m_playheads[i].attached = false;
    attachPlayhead(int(i));
  }

  // Every slot of the new arena starts empty.
  m_benches.clear();
  m_freeBenches.clear();
  m_benchOfNode.clear();
  m_benchOfProcess.clear();
  syncBenches();
}

void Telemetry::teardown(bool notify)
{
  auto& ctx = m_plugin.contextData();
  if(ctx && m_arena)
  {
    for(std::size_t i = 0; i < m_subs.size(); i++)
      detach(i);

    ctx->m_execQueue.enqueue(
        [c = std::weak_ptr{ctx}, arena = std::shared_ptr<ossia::telemetry::arena>{}]() mutable {
      if(auto ctx = c.lock())
        std::swap(ctx->telemetry, arena);
    });
  }
  m_arena.reset();
  for(auto& p : m_playheads)
    p.attached = false;
  if(notify)
    updated();
}

void Telemetry::attach(int index)
{
  auto& ctx = m_plugin.contextData();
  auto& s = m_subs[index];
  if(!ctx || !m_arena || s.attached || s.users == 0)
    return;

  // Made once it is known where it goes: read() retries until then.
  std::shared_ptr<ossia::telemetry::meter_tap> tap;
  if(s.kind == tap_kind::node && s.inlet)
  {
    if(!s.port)
      return;
    auto& inlets = ctx->setupContext.inlets;
    auto it = inlets.find(
        static_cast<Process::Inlet*>(const_cast<Process::Port*>(s.port.data())));
    if(it == inlets.end() || !it->second.first || !it->second.second
       || it->second.second->which() != ossia::audio_port::which)
      return; // Not executing yet: read() retries.

    tap = std::make_shared<ossia::telemetry::meter_tap>();
    auto node = it->second.first;
    auto in = static_cast<ossia::audio_inlet*>(it->second.second);
    s.node = node;
    s.ossia_inlet = in;
    ctx->m_execQueue.enqueue(
        [arena = m_arena, index, gen = s.generation, in_arena = tap, in_port = tap,
         node = std::move(node), in]() mutable {
      arena->attach(index, gen, tap_kind::node, in_arena);
      std::swap(in->meter, in_port);
    });
  }
  else if(s.kind == tap_kind::node && !s.param)
  {
    if(!s.port)
      return;
    auto& outlets = ctx->setupContext.outlets;
    auto it = outlets.find(
        static_cast<Process::Outlet*>(const_cast<Process::Port*>(s.port.data())));
    if(it == outlets.end() || !it->second.first || !it->second.second
       || it->second.second->which() != ossia::audio_port::which)
      return; // Not executing yet: read() retries.

    tap = std::make_shared<ossia::telemetry::meter_tap>();
    auto node = it->second.first;
    auto out = static_cast<ossia::audio_outlet*>(it->second.second);
    s.node = node;
    s.ossia_outlet = out;
    ctx->m_execQueue.enqueue(
        [arena = m_arena, index, gen = s.generation, in_arena = tap, in_port = tap,
         node = std::move(node), out]() mutable {
      arena->attach(index, gen, tap_kind::node, in_arena);
      std::swap(out->meter, in_port);
    });
  }
  else if(s.param)
  {
    // A virtual port: the parameter only gets the address of the tap, which
    // the arena owns from the next tick on and the closure until then.
    tap = std::make_shared<ossia::telemetry::meter_tap>();
    auto* raw = tap.get();
    ctx->m_execQueue.enqueue(
        [arena = m_arena, index, gen = s.generation,
         in_arena = std::move(tap)]() mutable {
      arena->attach(index, gen, tap_kind::node, in_arena);
    });
    s.param->meter.store(raw, std::memory_order_release);
  }
  else
  {
    tap = std::make_shared<ossia::telemetry::meter_tap>();
    ctx->m_execQueue.enqueue(
        [arena = m_arena, index, gen = s.generation, kind = s.kind,
         in_arena = std::move(tap)]() mutable {
      arena->attach(index, gen, kind, in_arena);
    });
  }
  s.attached = true;
}

void Telemetry::detach(int index)
{
  auto& ctx = m_plugin.contextData();
  auto& s = m_subs[index];
  // The tap stays alive until the closure below has run on the audio thread,
  // which is after anything that read this address is done with it.
  if(s.param)
    s.param->meter.store(nullptr, std::memory_order_release);
  if(!ctx || !m_arena || !s.attached)
    return;

  auto node = s.node.lock();
  ctx->m_execQueue.enqueue(
      [arena = m_arena, index, node = std::move(node), in = s.ossia_inlet,
       out = s.ossia_outlet, from_arena = std::shared_ptr<ossia::telemetry::meter_tap>{},
       from_port = std::shared_ptr<ossia::telemetry::meter_tap>{}]() mutable {
    arena->attach(index, 0, tap_kind::none, from_arena);
    if(node && in)
      std::swap(in->meter, from_port);
    else if(node && out)
      std::swap(out->meter, from_port);
  });
  s.attached = false;
  s.node.reset();
  s.ossia_inlet = nullptr;
  s.ossia_outlet = nullptr;
}

void Telemetry::read()
{
  if(!m_arena)
    return;

  for(std::size_t i = 0; i < m_subs.size(); i++)
    if(m_subs[i].users > 0 && !m_subs[i].attached)
      attach(i);

  if(!syncBenches())
  {
    // More process nodes than the arena has room for.
    rebuild();
    return;
  }

  if(m_arena->consume())
  {
    reportBenches();
    updated();
  }
}
}
