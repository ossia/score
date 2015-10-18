#include "MoveConstraint.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

#include "Process/Algorithms/VerticalMovePolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;


MoveConstraint::MoveConstraint(
        Path<ScenarioModel>&& scenarioPath,
        const Id<ConstraintModel>& id,
        const TimeValue& date,
        double height) :
    SerializableCommand{"ScenarioControl",
                        commandName(),
                        description()},
    m_path{std::move(scenarioPath)},
    m_constraint{id},
    m_newHeight{height}
{
    auto& scenar = m_path.find();
    auto& cst = scenar.constraints.at(m_constraint);

    m_oldHeight = cst.heightPercentage();
}

void MoveConstraint::update(const Path<ScenarioModel>& path,
                            const Id<ConstraintModel>&,
                            const TimeValue& date,
                            double height)
{
    m_newHeight = height;
}

void MoveConstraint::undo() const
{
    updateConstraintVerticalPos(
                m_oldHeight,
                m_constraint,
                m_path.find());
}

void MoveConstraint::redo() const
{
    updateConstraintVerticalPos(
                m_newHeight,
                m_constraint,
                m_path.find());
}

void MoveConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_constraint
      << m_oldHeight
      << m_newHeight;
}

void MoveConstraint::deserializeImpl(QDataStream& s)
{
    s >> m_path
      >> m_constraint
      >> m_oldHeight
      >> m_newHeight;
}
