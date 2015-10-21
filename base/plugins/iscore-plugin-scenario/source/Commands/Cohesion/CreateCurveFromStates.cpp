#include "CreateCurveFromStates.hpp"

#include "base/plugins/iscore-plugin-scenario/source/Document/Constraint/ConstraintModel.hpp"
#include "base/plugins/iscore-plugin-curve/Automation/AutomationModel.hpp"
#include "base/plugins/iscore-plugin-curve/Curve/CurveModel.hpp"
#include "base/plugins/iscore-plugin-curve/Curve/Segment/Linear/LinearCurveSegmentModel.hpp"

#include "Curve/Segment/Power/PowerCurveSegmentModel.hpp"
CreateCurveFromStates::CreateCurveFromStates(
        Path<ConstraintModel>&& constraint,
        const std::vector<Path<SlotModel>>& slotList,
        const iscore::Address& address,
        double start,
        double end):
    iscore::SerializableCommand{
        factoryName(),
        commandName(),
        description()},
    m_addProcessCmd{
        std::move(constraint),
        "Automation"},
    m_address(address),
    m_start{start},
    m_end{end}
{
    auto vec = m_addProcessCmd.constraintPath().unsafePath().vec();
    vec.push_back({"Automation", m_addProcessCmd.processId()});
    Path<Process> proc{ObjectPath{std::move(vec)}, Path<Process>::UnsafeDynamicCreation{}};
    m_slotsCmd.reserve(slotList.size());
    for(const auto& elt : slotList)
    {
        m_slotsCmd.emplace_back(Path<SlotModel>(elt), Path<Process>(proc), "Automation");
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
    auto segment = new DefaultCurveSegmentModel(Id<CurveSegmentModel>(0), &autom.curve());

    if(m_start != m_end)
    {
        segment->setStart({0., qreal(m_start > m_end)}); // Biggest is 1
        segment->setEnd({1., qreal(m_end > m_start)});
        autom.setMin(std::min(m_start, m_end));
        autom.setMax(std::max(m_start, m_end));
    }
    else
    {
        segment->setStart({0., 0.5});
        segment->setEnd({1., 0.5});
        autom.setMin(m_start);
        autom.setMax(m_start);
    }

    autom.curve().addSegment(segment);

    emit autom.curve().changed();

    for(const auto& cmd : m_slotsCmd)
        cmd.redo();
}

void CreateCurveFromStates::serializeImpl(QDataStream& s) const
{
    s << m_addProcessCmd.serialize() << m_address << m_start << m_end;
}

void CreateCurveFromStates::deserializeImpl(QDataStream& s)
{
    QByteArray a;
    s >> a >> m_address >> m_start >> m_end;

    m_addProcessCmd.deserialize(a);
}
