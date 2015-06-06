#include "CreateCurveFromStates.hpp"

#include "base/plugins/iscore-plugin-scenario/source/Commands/Constraint/AddProcessToConstraint.hpp"
#include "base/plugins/iscore-plugin-scenario/source/Document/Constraint/ConstraintModel.hpp"
#include "base/plugins/iscore-plugin-curve/Automation/AutomationModel.hpp"
#include "base/plugins/iscore-plugin-curve/Curve/CurveModel.hpp"
#include "base/plugins/iscore-plugin-curve/Curve/Segment/LinearCurveSegmentModel.hpp"
CreateCurveFromStates::CreateCurveFromStates():
    iscore::SerializableCommand{
        "IScoreCohesionControl",
        className(),
        description()}
{

}

CreateCurveFromStates::~CreateCurveFromStates()
{
    delete m_addProcessCmd;
}

CreateCurveFromStates::CreateCurveFromStates(
        ObjectPath&& constraint,
        QString address,
        double start,
        double end):
    // TODO to prevent needless recopying, why not templating the ctor
    // of SerializableCommand so that it takes T::className() and T::description() ?
    iscore::SerializableCommand{
        "IScoreCohesionControl",
        className(),
        description()},
    m_address{address},
    m_start{start},
    m_end{end}
{
    m_addProcessCmd = new Scenario::Command::AddProcessToConstraint{
        std::move(constraint),
        "Automation"};
}

void CreateCurveFromStates::undo()
{
    m_addProcessCmd->undo();
}

void CreateCurveFromStates::redo()
{
    m_addProcessCmd->redo();
    auto& cstr = m_addProcessCmd->constraintPath().find<ConstraintModel>();
    auto autom = static_cast<AutomationModel*>(cstr.process(m_addProcessCmd->processId()));
    autom->setAddress(m_address);
    autom->curve().clear();
    autom->setMin(std::min(m_start, m_end));
    autom->setMax(std::max(m_start, m_end));

    // Add a segment
    auto segment = new LinearCurveSegmentModel(id_type<CurveSegmentModel>(0), &autom->curve());
    segment->setStart({0., qreal(m_start > m_end)}); // Biggest is 1
    segment->setEnd({1., qreal(m_end > m_start)});

    autom->curve().addSegment(segment);
}

void CreateCurveFromStates::serializeImpl(QDataStream& s) const
{
    s << m_addProcessCmd->serialize() << m_address << m_start << m_end;
}

void CreateCurveFromStates::deserializeImpl(QDataStream& s)
{
    QByteArray a;
    s >> a >> m_address >> m_start >> m_end;

    m_addProcessCmd->deserialize(a);
}
