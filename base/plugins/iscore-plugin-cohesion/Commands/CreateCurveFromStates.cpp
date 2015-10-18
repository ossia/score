#include "CreateCurveFromStates.hpp"

#include "base/plugins/iscore-plugin-scenario/source/Document/Constraint/ConstraintModel.hpp"
#include "base/plugins/iscore-plugin-curve/Automation/AutomationModel.hpp"
#include "base/plugins/iscore-plugin-curve/Curve/CurveModel.hpp"
#include "base/plugins/iscore-plugin-curve/Curve/Segment/Linear/LinearCurveSegmentModel.hpp"

CreateCurveFromStates::CreateCurveFromStates(
        Path<ConstraintModel>&& constraint,
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
}

void CreateCurveFromStates::undo() const
{
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
    auto segment = new LinearCurveSegmentModel(Id<CurveSegmentModel>(0), &autom.curve());

    if(m_start != m_end)
    {
        segment->setStart({0., qreal(m_start > m_end)}); // Biggest is 1
        segment->setEnd({1., qreal(m_end > m_start)});
        autom.setMin(std::min(m_start, m_end));
        autom.setMax(std::max(m_start, m_end));
    }
    else
    {
        segment->setStart({0., m_start});
        segment->setStart({0., m_start});
        autom.setMin(m_start);
        autom.setMax(m_start);
    }

    autom.curve().addSegment(segment);
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
