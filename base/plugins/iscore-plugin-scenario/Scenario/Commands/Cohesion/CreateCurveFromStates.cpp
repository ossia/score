#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Power/PowerCurveSegmentModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <boost/optional/optional.hpp>


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
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class LayerModel;
class SlotModel;
namespace iscore {
struct Address;
}  // namespace iscore

CreateCurveFromStates::CreateCurveFromStates(Path<ConstraintModel>&& constraint, const std::vector<std::pair<Path<SlotModel>, Id<LayerModel> > >& slotList, const Id<Process>& curveId, const iscore::Address& address, double start, double end, double min, double max):
    CreateProcessAndLayers<AutomationProcessMetadata>{
        std::move(constraint),
        slotList,
        curveId},
    m_address(address),
    m_start{start},
    m_end{end},
    m_min{min},
    m_max{max}
{
}

void CreateCurveFromStates::redo() const
{
    m_addProcessCmd.redo();
    auto& cstr = m_addProcessCmd.constraintPath().find();
    auto& autom = safe_cast<AutomationModel&>(cstr.processes.at(m_addProcessCmd.processId()));
    autom.setAddress(m_address);
    autom.curve().clear();

    // Add a segment
    auto segment = new DefaultCurveSegmentModel{
                   Id<CurveSegmentModel>{iscore::id_generator::getFirstId()},
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

void CreateCurveFromStates::serializeImpl(DataStreamInput& s) const
{
    CreateProcessAndLayers<AutomationProcessMetadata>::serializeImpl(s);
    s << m_address << m_start << m_end << m_min << m_max;
}

void CreateCurveFromStates::deserializeImpl(DataStreamOutput& s)
{
    CreateProcessAndLayers<AutomationProcessMetadata>::deserializeImpl(s);
    s >> m_address >> m_start >> m_end >> m_min >> m_max;
}
