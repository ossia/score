#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Power/PowerCurveSegmentModel.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <boost/optional/optional.hpp>
#include <core/application/ApplicationComponents.hpp>

#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QByteArray>

#include <Automation/AutomationProcessMetadata.hpp>
#include "CreateCurveFromStates.hpp"
#include <Curve/Segment/CurveSegmentModel.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <core/application/ApplicationContext.hpp>
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

CreateCurveFromStates::CreateCurveFromStates(
        Path<ConstraintModel>&& constraint,
        const std::vector<std::pair<Path<SlotModel>, Id<LayerModel>>>& slotList,
        const Id<Process>& curveId,
        const iscore::Address& address,
        double start,
        double end,
        double min, double max):
    m_addProcessCmd{
        std::move(constraint),
        curveId,
        AutomationProcessMetadata::factoryKey()},
    m_address(address),
    m_start{start},
    m_end{end},
    m_min{min},
    m_max{max}
{
    auto proc = m_addProcessCmd.constraintPath().extend(AutomationProcessMetadata::processObjectName(), curveId);

    m_slotsCmd.reserve(slotList.size());

    auto fact = context.components.factory<ProcessList>().list().get(AutomationProcessMetadata::factoryKey());
    ISCORE_ASSERT(fact);
    auto procData = fact->makeStaticLayerConstructionData();

    for(const auto& elt : slotList)
    {
        m_slotsCmd.emplace_back(
                    Path<SlotModel>(elt.first),
                    elt.second,
                    Path<Process>{proc},
                    procData);
    }
}

void CreateCurveFromStates::undo() const
{
    for(const auto& cmd : m_slotsCmd)
        cmd.undo();
    m_addProcessCmd.undo();
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

    segment->setStart({0., (m_start - m_min) / (m_max - m_min) });
    segment->setEnd({1., (m_end - m_min) / (m_max - m_min) });

    autom.setMin(m_min);
    autom.setMax(m_max);

    autom.curve().addSegment(segment);

    emit autom.curve().changed();

    for(const auto& cmd : m_slotsCmd)
        cmd.redo();
}

void CreateCurveFromStates::serializeImpl(DataStreamInput& s) const
{
    s << m_addProcessCmd.serialize();
    s << (int)m_slotsCmd.size();
    for(const auto& elt : m_slotsCmd)
    {
        s << elt.serialize();
    }
    s << m_address << m_start << m_end << m_min << m_max;
}

void CreateCurveFromStates::deserializeImpl(DataStreamOutput& s)
{
    QByteArray a;
    s >> a;
    m_addProcessCmd.deserialize(a);

    int n = 0;
    s >> n;
    m_slotsCmd.resize(n);
    for(int i = 0; i < n; i++)
    {
        QByteArray b;
        s >> b;
        m_slotsCmd.at(i).deserialize(b);
    }

    s >> m_address >> m_start >> m_end >> m_min >> m_max;
}
