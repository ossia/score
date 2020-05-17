#include "AddProcessToInterval.hpp"

namespace Scenario
{
namespace Command
{
AddProcessToInterval::AddProcessToInterval(
    const Scenario::IntervalModel& interval,
    const UuidKey<Process::ProcessModel>& process,
    const QString& dat,
    const QPointF& pos)
    : m_addProcessCommand{interval, getStrongId(interval.processes), process, dat, pos}
    , m_addedSlot{interval.smallView().empty()}
{
}

void AddProcessToInterval::undo(const score::DocumentContext& ctx) const
{
  auto& interval = m_addProcessCommand.intervalPath().find(ctx);

  if (m_addedSlot)
    interval.removeSlot(0);
  else
    interval.removeLayer(0, m_addProcessCommand.processId());

  m_addProcessCommand.undo(ctx);
}

void AddProcessToInterval::redo(const score::DocumentContext& ctx) const
{
  auto& interval = m_addProcessCommand.intervalPath().find(ctx);

  // Create process model
  auto& proc = m_addProcessCommand.redo(interval, ctx);

  // Make it visible
  if (m_addedSlot)
  {
    auto h = score::AppContext().settings<Scenario::Settings::Model>().getSlotHeight();
    interval.addSlot(Slot{{proc.id()}, proc.id(), h});
    interval.setSmallViewVisible(true);
  }
  else
  {
    interval.addLayer(0, proc.id());
  }
}

const Path<Scenario::IntervalModel>& AddProcessToInterval::intervalPath() const
{
  return m_addProcessCommand.intervalPath();
}

const Id<Process::ProcessModel>& AddProcessToInterval::processId() const
{
  return m_addProcessCommand.processId();
}

const UuidKey<Process::ProcessModel>& AddProcessToInterval::processKey() const
{
  return m_addProcessCommand.processKey();
}

void AddProcessToInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_addProcessCommand.serialize() << m_addedSlot;
}

void AddProcessToInterval::deserializeImpl(DataStreamOutput& s)
{
  QByteArray b;
  s >> b >> m_addedSlot;

  m_addProcessCommand.deserialize(b);
}

AddProcessToInterval::~AddProcessToInterval() { }

LoadProcessInInterval::LoadProcessInInterval(
    const Scenario::IntervalModel& interval,
    const rapidjson::Value& dat)
    : m_addProcessCommand{interval, getStrongId(interval.processes), dat}
    , m_addedSlot{interval.smallView().empty()}
{
}

void LoadProcessInInterval::undo(const score::DocumentContext& ctx) const
{
  auto& interval = m_addProcessCommand.intervalPath().find(ctx);

  if (m_addedSlot)
    interval.removeSlot(0);
  else
    interval.removeLayer(0, m_addProcessCommand.processId());

  m_addProcessCommand.undo(ctx);
}

void LoadProcessInInterval::redo(const score::DocumentContext& ctx) const
{
  auto& interval = m_addProcessCommand.intervalPath().find(ctx);

  // Create process model
  auto& proc = m_addProcessCommand.redo(interval, ctx);

  // Make it visible
  if (m_addedSlot)
  {
    auto h = score::AppContext().settings<Scenario::Settings::Model>().getSlotHeight();
    interval.addSlot(Slot{{proc.id()}, proc.id(), h});
    interval.setSmallViewVisible(true);
  }
  else
  {
    interval.addLayer(0, proc.id());
  }
}

const Path<Scenario::IntervalModel>& LoadProcessInInterval::intervalPath() const
{
  return m_addProcessCommand.intervalPath();
}

const Id<Process::ProcessModel>& LoadProcessInInterval::processId() const
{
  return m_addProcessCommand.processId();
}

void LoadProcessInInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_addProcessCommand.serialize() << m_addedSlot;
}

void LoadProcessInInterval::deserializeImpl(DataStreamOutput& s)
{
  QByteArray b;
  s >> b >> m_addedSlot;

  m_addProcessCommand.deserialize(b);
}

LoadProcessInInterval::~LoadProcessInInterval() { }

AddProcessInNewBoxMacro::~AddProcessInNewBoxMacro() { }
}
}
