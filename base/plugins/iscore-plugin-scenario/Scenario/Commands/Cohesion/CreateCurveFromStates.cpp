#include "CreateCurveFromStates.hpp"

#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <Automation/AutomationModel.hpp>
#include <Curve/CurveModel.hpp>
#include <Curve/Segment/Linear/LinearCurveSegmentModel.hpp>

#include <Curve/Segment/Power/PowerCurveSegmentModel.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>

CreateCurveFromStates::CreateCurveFromStates(
        Path<ConstraintModel>&& constraint,
        const std::vector<std::pair<Path<SlotModel>, Id<LayerModel>>>& slotList,
        const Id<Process>& curveId,
        const iscore::Address& address,
        double start,
        double end,
        double min, double max):
    iscore::SerializableCommand{
        factoryName(),
        commandName(),
        description()},
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
    // TODO REFACTORME (grep for UnsafeDynamicCreation)
    auto vec = m_addProcessCmd.constraintPath().unsafePath().vec();
    vec.push_back({AutomationProcessMetadata::processObjectName(), curveId});
    Path<Process> proc{ObjectPath{std::move(vec)}, Path<Process>::UnsafeDynamicCreation{}};

    m_slotsCmd.reserve(slotList.size());

    for(const auto& elt : slotList)
    {
        m_slotsCmd.emplace_back(
                    Path<SlotModel>(elt.first),
                    elt.second,
                    Path<Process>(proc),
                    AutomationProcessMetadata::factoryKey());
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

void CreateCurveFromStates::serializeImpl(QDataStream& s) const
{
    s << m_addProcessCmd.serialize();
    s << (int)m_slotsCmd.size();
    for(const auto& elt : m_slotsCmd)
    {
        s << elt.serialize();
    }
    s << m_address << m_start << m_end << m_min << m_max;
}

void CreateCurveFromStates::deserializeImpl(QDataStream& s)
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
        m_slotsCmd.at(n).deserialize(b);
    }

    s >> m_address >> m_start >> m_end >> m_min >> m_max;
}
