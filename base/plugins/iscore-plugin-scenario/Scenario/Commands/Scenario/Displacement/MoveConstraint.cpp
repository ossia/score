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

    auto list = selectedElements(scenar.constraints);

    for (auto& elt : list)
    {
        m_selectedConstraints.append({elt->id(), elt->heightPercentage()});
    }

    if(m_selectedConstraints.empty())
        m_selectedConstraints.append({m_constraint, m_oldHeight});

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
    auto& scenar = m_path.find();
    for (auto cstr : m_selectedConstraints)
    {
        updateConstraintVerticalPos(
                    cstr.second,
                    cstr.first,
                    scenar);
    }
}

void MoveConstraint::redo() const
{
    auto& scenar = m_path.find();
    for (auto cstr : m_selectedConstraints)
    {
        updateConstraintVerticalPos(
                    cstr.second + m_newHeight - m_oldHeight,
                    cstr.first,
                    scenar);
    }
}

void MoveConstraint::serializeImpl(DataStreamInput& s) const
{
    s << m_path
      << m_constraint
      << m_oldHeight
      << m_newHeight
      << m_selectedConstraints;
}

void MoveConstraint::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path
      >> m_constraint
      >> m_oldHeight
      >> m_newHeight
      >> m_selectedConstraints;
}
}
}
