#include "MoveConstraint.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;

// @todo : maybe should we use deplacement value and not absolute ending point.
// @todo : don't allow too small translation on t axis, so user can move a constraint only on vertical, without changing any duration.

MoveConstraint::MoveConstraint(ObjectPath&& scenarioPath,
                               const id_type<ConstraintModel>& id,
                               const TimeValue& date,
                               double y) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
m_path {std::move(scenarioPath) },
m_constraintId {id},
m_newHeightPosition {y},
m_newX {date}
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
            m_oldHeightPosition,
            [] (ProcessSharedModelInterface* p, TimeValue t) { p->setDurationAndScale(t); });
}

void MoveConstraint::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    StandardDisplacementPolicy::setConstraintPosition(*scenar,
            m_constraintId,
            m_newX,
            m_newHeightPosition,
            [] (ProcessSharedModelInterface* p, TimeValue t) { p->setDurationAndScale(t); });
}

bool MoveConstraint::mergeWith(const Command* other)
{
    // Maybe set m_mergeable = false at the end ?
    if(other->uid() != uid())
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
