#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <iscore/tools/std/Optional.hpp>

#include <QByteArray>
#include <iscore/tools/IdentifierGeneration.hpp>

#include "CreateCurveFromStates.hpp"
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
namespace Command
{
CreateAutomationFromStates::CreateAutomationFromStates(
    const ConstraintModel& constraint,
    const std::vector<Path<SlotModel>>& slotList,
    Id<Process::ProcessModel> curveId,
    State::AddressAccessor address,
    const Curve::CurveDomain& dom)
    : CreateProcessAndLayers<Automation::ProcessModel>{constraint, slotList,
                                                       std::move(curveId)}
    , m_address{std::move(address)}
    , m_dom{dom}
{
}

void CreateAutomationFromStates::redo() const
{
  m_addProcessCmd.redo();
  auto& cstr = m_addProcessCmd.constraintPath().find();
  auto& autom = safe_cast<Automation::ProcessModel&>(
      cstr.processes.at(m_addProcessCmd.processId()));
  autom.setAddress(m_address);
  autom.curve().clear();

  // Add a segment
  auto segment = new Curve::DefaultCurveSegmentModel{
      Id<Curve::SegmentModel>{iscore::id_generator::getFirstId()},
      &autom.curve()};

  double fact = 1. / (m_dom.max - m_dom.min);
  segment->setStart({0., (m_dom.start - m_dom.min) * fact});
  segment->setEnd({1., (m_dom.end - m_dom.min) * fact});

  autom.setMin(m_dom.min);
  autom.setMax(m_dom.max);

  autom.curve().addSegment(segment);

  emit autom.curve().changed();

  for (const auto& cmd : m_slotsCmd)
    cmd.redo();
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
    const ConstraintModel& constraint,
    const std::vector<Path<SlotModel>>&
        slotList,
    Id<Process::ProcessModel> curveId, State::AddressAccessor address,
    State::Value start, State::Value end)
    : CreateProcessAndLayers<Interpolation::ProcessModel>{constraint, slotList,
                                                          std::move(curveId)}
    , m_address{std::move(address)}
    , m_start{std::move(start)}
    , m_end{std::move(end)}
{
}

void CreateInterpolationFromStates::redo() const
{
  m_addProcessCmd.redo();

  auto& cstr = m_addProcessCmd.constraintPath().find();
  auto& autom = safe_cast<Interpolation::ProcessModel&>(
      cstr.processes.at(m_addProcessCmd.processId()));
  autom.setAddress(m_address);
  autom.setStart(m_start);
  autom.setEnd(m_end);

  for (const auto& cmd : m_slotsCmd)
    cmd.redo();
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
