#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Power/PowerSegment.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <iscore/tools/std/Optional.hpp>


#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QByteArray>

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
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Scenario
{
namespace Command
{
CreateAutomationFromStates::CreateAutomationFromStates(
        const ConstraintModel& constraint,
        const std::vector<std::pair<Path<SlotModel>, Id<Process::LayerModel> > >& slotList,
        Id<Process::ProcessModel> curveId,
        State::Address address,
        double start,
        double end,
        double min,
        double max):
    CreateProcessAndLayers<Automation::ProcessModel>{
        constraint,
        slotList,
        std::move(curveId)},
    m_address{std::move(address)},
    m_start{start},
    m_end{end},
    m_min{min},
    m_max{max}
{
}

void CreateAutomationFromStates::redo() const
{
    m_addProcessCmd.redo();
    auto& cstr = m_addProcessCmd.constraintPath().find();
    auto& autom = safe_cast<Automation::ProcessModel&>(cstr.processes.at(m_addProcessCmd.processId()));
    autom.setAddress(m_address);
    autom.curve().clear();

    // Add a segment
    auto segment = new Curve::DefaultCurveSegmentModel{
                   Id<Curve::SegmentModel>{iscore::id_generator::getFirstId()},
                   &autom.curve()};

    double fact = 1. / (m_max - m_min);
    segment->setStart({0., (m_start - m_min) * fact });
    segment->setEnd({1., (m_end - m_min) * fact });

    autom.setMin(m_min);
    autom.setMax(m_max);

    autom.curve().addSegment(segment);

    emit autom.curve().changed();

    for(const auto& cmd : m_slotsCmd)
        cmd.redo();
}

void CreateAutomationFromStates::serializeImpl(DataStreamInput& s) const
{
    CreateProcessAndLayers<Automation::ProcessModel>::serializeImpl(s);
    s << m_address << m_start << m_end << m_min << m_max;
}

void CreateAutomationFromStates::deserializeImpl(DataStreamOutput& s)
{
    CreateProcessAndLayers<Automation::ProcessModel>::deserializeImpl(s);
    s >> m_address >> m_start >> m_end >> m_min >> m_max;
}




CreateInterpolationFromStates::CreateInterpolationFromStates(
        const ConstraintModel& constraint,
        const std::vector<std::pair<Path<SlotModel>, Id<Process::LayerModel> > >& slotList,
        Id<Process::ProcessModel> curveId,
        State::Address address,
        State::Value start, State::Value end):
    CreateProcessAndLayers<Interpolation::ProcessModel>{
        constraint,
        slotList,
        std::move(curveId)},
    m_address{std::move(address)},
    m_start{std::move(start)},
    m_end{std::move(end)}
{
}

void CreateInterpolationFromStates::redo() const
{
    m_addProcessCmd.redo();

    auto& cstr = m_addProcessCmd.constraintPath().find();
    auto& autom = safe_cast<Interpolation::ProcessModel&>(cstr.processes.at(m_addProcessCmd.processId()));
    autom.setAddress(m_address);
    autom.setStart(m_start);
    autom.setEnd(m_end);

    for(const auto& cmd : m_slotsCmd)
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
