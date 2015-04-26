#include "MoveConstraint.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;

MoveConstraint::MoveConstraint(ObjectPath&& scenarioPath,
                               const id_type<ConstraintModel>& id,
                               const TimeValue& date,
                               double y,
                               ExpandMode mode) :
    SerializableCommand {"ScenarioControl",
                         className(),
                         description()},
    m_path {std::move(scenarioPath) },
    m_constraintId {id},
    m_newHeightPosition {y},
    m_newX {date},
    m_mode{mode}
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
                                                      [&] (ProcessSharedModelInterface* p, const TimeValue& t)
          { p->expandProcess(m_mode, t); });
}

void MoveConstraint::redo()
{
    auto scenar = m_path.find<ScenarioModel>();
    StandardDisplacementPolicy::setConstraintPosition(*scenar,
                                                      m_constraintId,
                                                      m_newX,
                                                      m_newHeightPosition,
                                                      [&] (ProcessSharedModelInterface* p, const TimeValue& t)
          { p->expandProcess(m_mode, t); });
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
      << m_oldHeightPosition << m_newHeightPosition << m_oldX << m_newX << (int)m_mode;
}

void MoveConstraint::deserializeImpl(QDataStream& s)
{
    int mode;
    s >> m_path >> m_constraintId
            >> m_oldHeightPosition >> m_newHeightPosition >> m_oldX >> m_newX >> mode;
    m_mode = static_cast<ExpandMode>(mode);
}
