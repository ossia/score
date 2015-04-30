#include "MoveEventAndConstraint.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

using namespace Scenario::Command;


MoveEventAndConstraint::MoveEventAndConstraint():
    iscore::SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
    m_cmd{new MoveEvent}
{

}

MoveEventAndConstraint::MoveEventAndConstraint(
        ObjectPath&& scenarioPath,
        id_type<ConstraintModel> constraintId,
        id_type<EventModel> eventId,
        const TimeValue& date,
        double height,
        ExpandMode mode):
    iscore::SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
    m_cmd{new MoveEvent{std::move(scenarioPath), eventId, date, height, mode}},
    m_constraintId{constraintId}
{

}


void MoveEventAndConstraint::undo()
{
    m_cmd->undo();
}


void MoveEventAndConstraint::redo()
{
    m_cmd->redo();

    auto scenar = m_cmd->path().find<ScenarioModel>();
    scenar->constraint(m_constraintId)->setHeightPercentage(m_cmd->heightPosition());
}


bool MoveEventAndConstraint::mergeWith(const iscore::Command*)
{
    return false;
}


void MoveEventAndConstraint::serializeImpl(QDataStream& s) const
{
    s << m_cmd->serialize() << m_constraintId;
}


void MoveEventAndConstraint::deserializeImpl(QDataStream& s)
{
    QByteArray a;
    s >> a >> m_constraintId;

    m_cmd->deserialize(a);
}
