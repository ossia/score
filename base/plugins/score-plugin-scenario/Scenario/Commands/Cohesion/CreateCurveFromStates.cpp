// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/tools/std/Optional.hpp>

#include <QByteArray>
#include <score/tools/IdentifierGeneration.hpp>

#include "CreateCurveFromStates.hpp"
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>

#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/Identifier.hpp>

namespace Scenario
{
namespace Command
{
CreateAutomationFromStates::CreateAutomationFromStates(
    const IntervalModel& interval,
    const std::vector<SlotPath>& slotList,
    Id<Process::ProcessModel> curveId,
    State::AddressAccessor address,
    const Curve::CurveDomain& dom,
    bool tween)
    : CreateProcessAndLayers<Automation::ProcessModel>{interval, slotList,
                                                       std::move(curveId)}
    , m_address{std::move(address)}
    , m_dom{dom}
    , m_tween(tween)
{
}

void CreateAutomationFromStates::redo(const score::DocumentContext& ctx) const
{
  m_addProcessCmd.redo(ctx);
  auto& cstr = m_addProcessCmd.intervalPath().find(ctx);
  auto& autom = safe_cast<Automation::ProcessModel&>(
      cstr.processes.at(m_addProcessCmd.processId()));
  autom.setAddress(m_address);
  autom.curve().clear();
  autom.setTween(m_tween);

  // Add a segment
  auto segment = new Curve::DefaultCurveSegmentModel{
      Id<Curve::SegmentModel>{score::id_generator::getFirstId()},
      &autom.curve()};

  double fact = 1. / (m_dom.max - m_dom.min);
  segment->setStart({0., (m_dom.start - m_dom.min) * fact});
  segment->setEnd({1., (m_dom.end - m_dom.min) * fact});

  autom.setMin(m_dom.min);
  autom.setMax(m_dom.max);

  autom.curve().addSegment(segment);

  emit autom.curve().changed();

  for (const auto& cmd : m_slotsCmd)
    cmd.redo(ctx);
}

void CreateAutomationFromStates::serializeImpl(DataStreamInput& s) const
{
  CreateProcessAndLayers<Automation::ProcessModel>::serializeImpl(s);
  s << m_address << m_dom;
}

void CreateAutomationFromStates::deserializeImpl(DataStreamOutput& s)
{
  CreateProcessAndLayers<Automation::ProcessModel>::deserializeImpl(s);
  s >> m_address >> m_dom;
}

CreateInterpolationFromStates::CreateInterpolationFromStates(
    const IntervalModel& interval,
    const std::vector<SlotPath>&
        slotList,
    Id<Process::ProcessModel> curveId, State::AddressAccessor address,
    ossia::value start, ossia::value end)
    : CreateProcessAndLayers<Interpolation::ProcessModel>{interval, slotList,
                                                          std::move(curveId)}
    , m_address{std::move(address)}
    , m_start{std::move(start)}
    , m_end{std::move(end)}
{
}

void CreateInterpolationFromStates::redo(const score::DocumentContext& ctx) const
{
  m_addProcessCmd.redo(ctx);

  auto& cstr = m_addProcessCmd.intervalPath().find(ctx);
  auto& autom = safe_cast<Interpolation::ProcessModel&>(
      cstr.processes.at(m_addProcessCmd.processId()));
  autom.setAddress(m_address);
  autom.setStart(m_start);
  autom.setEnd(m_end);

  for (const auto& cmd : m_slotsCmd)
    cmd.redo(ctx);
}

void CreateInterpolationFromStates::serializeImpl(DataStreamInput& s) const
{
  CreateProcessAndLayers<Interpolation::ProcessModel>::serializeImpl(s);
  s << m_address << m_start << m_end;
}

void CreateInterpolationFromStates::deserializeImpl(DataStreamOutput& s)
{
  CreateProcessAndLayers<Interpolation::ProcessModel>::deserializeImpl(s);
  s >> m_address >> m_start >> m_end;
}
}
}
