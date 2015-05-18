#include "CreateCurveFromStates.hpp"

#include "base/plugins/iscore-plugin-scenario/source/Commands/Constraint/AddProcessToConstraint.hpp"
#include "base/plugins/iscore-plugin-scenario/source/Document/Constraint/ConstraintModel.hpp"
#include "base/plugins/iscore-plugin-curve/Automation/AutomationModel.hpp"
CreateCurveFromStates::CreateCurveFromStates():
    iscore::SerializableCommand{
        "IScoreCohesionControl",
        className(),
        description()}
{

}

CreateCurveFromStates::~CreateCurveFromStates()
{
    delete m_cmd;
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
    m_cmd = new Scenario::Command::AddProcessToConstraint{
        std::move(constraint),
        "Automation"};
}

void CreateCurveFromStates::undo()
{
    m_cmd->undo();
}

void CreateCurveFromStates::redo()
{
    m_cmd->redo();
    auto& cstr = m_cmd->constraintPath().find<ConstraintModel>();
    auto autom = static_cast<AutomationModel*>(cstr.process(m_cmd->processId()));
    autom->setAddress(m_address);
    autom->movePoint(0, 0, m_start);
    autom->movePoint(1, 1, m_end);
}

void CreateCurveFromStates::serializeImpl(QDataStream& s) const
{
    s << m_cmd->serialize() << m_address << m_start << m_end;
}

void CreateCurveFromStates::deserializeImpl(QDataStream& s)
{
    QByteArray a;
    s >> a >> m_address >> m_start >> m_end;

    m_cmd->deserialize(a);
}
