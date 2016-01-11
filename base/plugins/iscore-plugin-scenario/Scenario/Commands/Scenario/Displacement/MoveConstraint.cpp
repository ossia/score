#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Process/Algorithms/VerticalMovePolicy.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <algorithm>

#include "MoveConstraint.hpp"
#include <Process/TimeValue.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/ModelPathSerialization.hpp>
#include <iscore/tools/NotifyingMap.hpp>

using namespace iscore;



namespace Scenario
{
namespace Command
{
MoveConstraint::MoveConstraint(
        Path<Scenario::ScenarioModel>&& scenarioPath,
        const Id<ConstraintModel>& id,
        const TimeValue& date,
        double height) :
    m_path{std::move(scenarioPath)},
    m_constraint{id},
    m_newHeight{height}
{
    auto& scenar = m_path.find();
    auto& cst = scenar.constraints.at(m_constraint);

    m_oldHeight = cst.heightPercentage();
}

void MoveConstraint::update(const Path<Scenario::ScenarioModel>& path,
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

void MoveConstraint::serializeImpl(DataStreamInput& s) const
{
    s << m_path
      << m_constraint
      << m_oldHeight
      << m_newHeight;
}

void MoveConstraint::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path
      >> m_constraint
      >> m_oldHeight
      >> m_newHeight;
}
}
}
