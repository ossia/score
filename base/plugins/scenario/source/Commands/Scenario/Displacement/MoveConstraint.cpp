#include "MoveConstraint.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

#include "Process/Algorithms/StandardDisplacementPolicy.hpp"
#include "MoveEvent.hpp"

using namespace iscore;
using namespace Scenario::Command;

MoveConstraint::MoveConstraint():
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_cmd{new MoveEvent}
{

}

MoveConstraint::~MoveConstraint()
{
    delete m_cmd;
}

MoveConstraint::MoveConstraint(ObjectPath&& scenarioPath,
                               const id_type<ConstraintModel>& id,
                               const TimeValue& date,
                               double height,
                               ExpandMode mode) :
    SerializableCommand{"ScenarioControl",
                        className(),
                        description()},
    m_path{std::move(scenarioPath)},
    m_constraint{id},
    m_newHeightPosition{height}
{
    auto scenar = m_path.find<ScenarioModel>();
    auto cst = scenar->constraint(m_constraint);

    auto event_id = cst->startEvent();
    m_cmd = new MoveEvent{
            ObjectPath{m_path},
            event_id,
            date,
            scenar->event(event_id)->heightPercentage(),
            mode};

    m_oldHeightPosition = cst->heightPercentage();
}

void MoveConstraint::undo()
{
    m_cmd->undo();

    auto scenar = m_path.find<ScenarioModel>();
    scenar->constraint(m_constraint)->setHeightPercentage(m_oldHeightPosition);
}

void MoveConstraint::redo()
{
    m_cmd->redo();

    auto scenar = m_path.find<ScenarioModel>();
    scenar->constraint(m_constraint)->setHeightPercentage(m_newHeightPosition);
}

bool MoveConstraint::mergeWith(const Command* other)
{
    // Maybe set m_mergeable = false at the end ?
    if(other->uid() != uid())
    {
        return false;
    }

    auto cmd = static_cast<const MoveConstraint*>(other);

    m_cmd->mergeWith(cmd->m_cmd);
    m_newHeightPosition = cmd->m_newHeightPosition;

    return true;
}

void MoveConstraint::serializeImpl(QDataStream& s) const
{
    s << m_cmd->serialize() << m_newHeightPosition;
}

void MoveConstraint::deserializeImpl(QDataStream& s)
{
    QByteArray a;
    s >> a >> m_newHeightPosition;

    m_cmd->deserialize(a);
}
