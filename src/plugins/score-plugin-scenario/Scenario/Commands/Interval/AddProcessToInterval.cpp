#include "AddProcessToInterval.hpp"

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

namespace Scenario
{
namespace Command
{
LoadLayerInInterval::LoadLayerInInterval(
    const Scenario::IntervalModel& interval,
    const rapidjson::Value& dat)
    : m_addProcessCommand{interval, getStrongId(interval.processes), dat}
    , m_addedSlot{interval.smallView().empty()}
{
}

void LoadLayerInInterval::undo(const score::DocumentContext& ctx) const
{
  auto& interval = m_addProcessCommand.intervalPath().find(ctx);

  if (m_addedSlot)
    interval.removeSlot(0);
  else
    interval.removeLayer(0, m_addProcessCommand.processId());

  m_addProcessCommand.undo(ctx);
}

void LoadLayerInInterval::redo(const score::DocumentContext& ctx) const
{
  auto& interval = m_addProcessCommand.intervalPath().find(ctx);

  // Create process model
  auto& proc = m_addProcessCommand.redo(interval, ctx);

  // Make it visible
  if (m_addedSlot)
  {
    const double h = Scenario::getNewLayerHeight(ctx.app, proc);
    interval.addSlot(Slot{{proc.id()}, proc.id(), h});
    interval.setSmallViewVisible(true);
  }
  else
  {
    interval.addLayer(0, proc.id());
  }
}

const Path<Scenario::IntervalModel>& LoadLayerInInterval::intervalPath() const
{
  return m_addProcessCommand.intervalPath();
}

const Id<Process::ProcessModel>& LoadLayerInInterval::processId() const
{
  return m_addProcessCommand.processId();
}

void LoadLayerInInterval::serializeImpl(DataStreamInput& s) const
{
  s << m_addProcessCommand.serialize() << m_addedSlot;
}

void LoadLayerInInterval::deserializeImpl(DataStreamOutput& s)
{
  QByteArray b;
  s >> b >> m_addedSlot;

  m_addProcessCommand.deserialize(b);
}

LoadLayerInInterval::~LoadLayerInInterval() { }

AddProcessInNewBoxMacro::~AddProcessInNewBoxMacro() { }
}
}
