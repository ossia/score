#include "MoveConstraint.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ConstraintData.hpp"

#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

#include <core/application/Application.hpp>
#include <core/view/View.hpp>

#include <QApplication>
#define CMD_UID 1201

using namespace iscore;
using namespace Scenario::Command;

// @todo : maybe should we use deplacement value and not absolute ending point.
// @todo : don't allow too small translation on t axis, so user can move a constraint only on vertical, without changing any duration.

MoveConstraint::MoveConstraint() :
    SerializableCommand {"ScenarioControl",
    "MoveConstraint",
    QObject::tr("Constraint move")
}
{
}

MoveConstraint::MoveConstraint(ObjectPath&& scenarioPath, ConstraintData d) :
    SerializableCommand {"ScenarioControl",
    "MoveConstraint",
    QObject::tr("Constraint move")
},
m_path {std::move(scenarioPath) },
m_constraintId {d.id},
m_newHeightPosition {d.relativeY},
m_newX {d.dDate}
{
    auto scenar = m_path.find<ScenarioModel>();
    auto cst = scenar->constraint(m_constraintId);
    m_oldHeightPosition = cst->heightPercentage();
    m_oldX = cst->startDate();
}

void MoveConstraint::undo()
{
    auto scenar = m_path.find<ScenarioModel>();

    StandardDisplacementPolicy::setConstraintPosition(*scenar,
            m_constraintId,
            m_oldX,
            m_oldHeightPosition);
}

void MoveConstraint::redo()
{
    auto scenar = m_path.find<ScenarioModel>();

    m_oldHeightPosition = scenar->constraint(m_constraintId)->heightPercentage();

    StandardDisplacementPolicy::setConstraintPosition(*scenar,
            m_constraintId,
            m_newX,
            m_newHeightPosition);
}

int MoveConstraint::id() const
{
    return canMerge() ? CMD_UID : -1;
}

bool MoveConstraint::mergeWith(const QUndoCommand* other)
{
    // Maybe set m_mergeable = false at the end ?
    if(other->id() != id())
    {
        return false;
    }

    auto cmd = static_cast<const MoveConstraint*>(other);
    m_newX = cmd->m_newX;
    m_newHeightPosition = cmd->m_newHeightPosition;

    return true;
}

void MoveConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path << m_constraintId
      << m_oldHeightPosition << m_newHeightPosition << m_oldX << m_newX;
}

void MoveConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_constraintId
      >> m_oldHeightPosition >> m_newHeightPosition >> m_oldX >> m_newX;
}
