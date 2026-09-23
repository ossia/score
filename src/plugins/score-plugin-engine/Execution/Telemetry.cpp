#include "Telemetry.hpp"

#include <Process/Dataflow/Port.hpp>

#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <Audio/Settings/Model.hpp>
#include <Execution/DocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

#include <QTimer>

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

Telemetry::~Telemetry() = default;

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

Telemetry::Meter
Telemetry::subscribe(tap_kind kind, const Process::AudioOutlet* outlet)
{
  for(std::size_t i = 0; i < m_subs.size(); i++)
  {
    auto& s = m_subs[i];
    if(s.users > 0 && s.kind == kind && s.outlet == outlet)
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
  s.outlet = outlet;
  s.generation = ++generations;
  s.users = 1;

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
  return subscribe(tap_kind::node, &outlet);
}

Telemetry::Meter Telemetry::meterHardwareInputs()
{
  return subscribe(tap_kind::hardware_inputs, nullptr);
}

Telemetry::Meter Telemetry::meterHardwareOutputs()
{
  return subscribe(tap_kind::hardware_outputs, nullptr);
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

void Telemetry::executionStarted()
{
  m_running = true;
  if(enabled())
    rebuild();
}

void Telemetry::executionStopped()
{
  // The graph goes away with its outlets and its context: nothing to detach.
  m_running = false;
  m_arena.reset();
  for(auto& s : m_subs)
  {
    s.attached = false;
    s.node.reset();
    s.ossia_outlet = nullptr;
  }
  updated();
}

void Telemetry::rebuild()
{
  auto& ctx = m_plugin.contextData();
  if(!ctx)
    return;

  teardown();

  const std::size_t capacity = std::max<std::size_t>(64, 2 * m_subs.size());
  m_arena = std::make_shared<ossia::telemetry::arena>(capacity);
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
}

void Telemetry::teardown()
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
  updated();
}

void Telemetry::attach(int index)
{
  auto& ctx = m_plugin.contextData();
  auto& s = m_subs[index];
  if(!ctx || !m_arena || s.attached || s.users == 0)
    return;

  auto tap = std::make_shared<ossia::telemetry::meter_tap>();
  if(s.kind == tap_kind::node)
  {
    if(!s.outlet)
      return;
    auto& outlets = ctx->setupContext.outlets;
    auto it = outlets.find(const_cast<Process::AudioOutlet*>(s.outlet.data()));
    if(it == outlets.end() || !it->second.first || !it->second.second
       || it->second.second->which() != ossia::audio_port::which)
      return; // Not executing yet: read() retries.

    auto node = it->second.first;
    auto out = static_cast<ossia::audio_outlet*>(it->second.second);
    s.node = node;
    s.ossia_outlet = out;
    ctx->m_execQueue.enqueue(
        [arena = m_arena, index, gen = s.generation, in_arena = tap, in_outlet = tap,
         node = std::move(node), out]() mutable {
      arena->attach(index, gen, tap_kind::node, in_arena);
      std::swap(out->meter, in_outlet);
    });
  }
  else
  {
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
  if(!ctx || !m_arena || !s.attached)
    return;

  auto node = s.node.lock();
  ctx->m_execQueue.enqueue(
      [arena = m_arena, index, node = std::move(node), out = s.ossia_outlet,
       from_arena = std::shared_ptr<ossia::telemetry::meter_tap>{},
       from_outlet = std::shared_ptr<ossia::telemetry::meter_tap>{}]() mutable {
    arena->attach(index, 0, tap_kind::none, from_arena);
    if(node)
      std::swap(out->meter, from_outlet);
  });
  s.attached = false;
  s.node.reset();
  s.ossia_outlet = nullptr;
}

void Telemetry::read()
{
  if(!m_arena)
    return;

  for(std::size_t i = 0; i < m_subs.size(); i++)
    if(m_subs[i].users > 0 && !m_subs[i].attached)
      attach(i);

  if(m_arena->consume())
    updated();
}
}
