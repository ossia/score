// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SwapSlots.hpp"

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Settings/ScenarioSettingsModel.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Scenario
{
namespace Command
{
ChangeSlotPosition::ChangeSlotPosition(
    Path<IntervalModel>&& rack,
    Slot::RackView v,
    int first,
    int second)
    : m_path{std::move(rack)}, m_view{v}, m_first{std::move(first)}, m_second{std::move(second)}
{
}

void ChangeSlotPosition::undo(const score::DocumentContext& ctx) const
{
  redo(ctx);
}

void ChangeSlotPosition::redo(const score::DocumentContext& ctx) const
{
  auto& cst = m_path.find(ctx);
  cst.swapSlots(m_first, m_second, m_view);
}

void ChangeSlotPosition::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_view << m_first << m_second;
}

void ChangeSlotPosition::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_view >> m_first >> m_second;
}

SlotCommand::SlotCommand(const IntervalModel& c)
    : m_path{c}, m_old{c.smallView()}, m_new{m_old} { }

void SlotCommand::undo(const score::DocumentContext& ctx) const
{
  auto& cst = m_path.find(ctx);
  cst.replaceSmallView(m_old);
}

void SlotCommand::redo(const score::DocumentContext& ctx) const
{
  auto& cst = m_path.find(ctx);
  cst.replaceSmallView(m_new);
}

void SlotCommand::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_old << m_new;
}

void SlotCommand::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_old >> m_new;
}

MergeSlots::MergeSlots(const IntervalModel& rack, int first, int second) : SlotCommand{rack}
{
  auto& source = m_old[first];
  auto& target = m_new[second];
  target.frontProcess = source.frontProcess;
  target.processes.insert(
      target.processes.end(), source.processes.begin(), source.processes.end());
  m_new.erase(m_new.begin() + first);
}

MoveLayerInNewSlot::MoveLayerInNewSlot(const IntervalModel& rack, int first, int second)
    : SlotCommand{rack}
{
  auto source = m_old[first];
  Scenario::Slot newSlot;
  newSlot.processes.push_back(*source.frontProcess);
  newSlot.frontProcess = *source.frontProcess;
  newSlot.height = score::AppContext().settings<Scenario::Settings::Model>().getSlotHeight();

  auto it = ossia::find(source.processes, *source.frontProcess);
  SCORE_ASSERT(it != source.processes.end());
  source.processes.erase(it);

  if (source.processes.empty())
  {
    m_new.insert(m_new.begin() + second, newSlot);
    if (first < second)
    {
      m_new.erase(m_new.begin() + first);
    }
    else if (first > second)
    {
      m_new.erase(m_new.begin() + first + 1);
    }
  }
  else
  {
    source.frontProcess = source.processes.front();
    m_new[first] = source;
    m_new.insert(m_new.begin() + second, newSlot);
  }
}

MergeLayerInSlot::MergeLayerInSlot(const IntervalModel& rack, int first, int second)
    : SlotCommand{rack}
{
  auto source = m_old[first];
  auto target = m_old[second];

  target.processes.push_back(*source.frontProcess);
  target.frontProcess = source.frontProcess;

  auto it = ossia::find(source.processes, *source.frontProcess);
  SCORE_ASSERT(it != source.processes.end());
  source.processes.erase(it);

  if (source.processes.empty())
  {
    m_new[second] = target;
    m_new.erase(m_new.begin() + first);
  }
  else
  {
    source.frontProcess = source.processes.front();
    m_new[first] = source;
    m_new[second] = target;
  }
}
}
}
