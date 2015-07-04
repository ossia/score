#include "MoveConstraint.hpp"

#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

#include "Process/Algorithms/StandardDisplacementPolicy.hpp"

using namespace iscore;
using namespace Scenario::Command;

MoveConstraint::MoveConstraint():
    SerializableCommand {"ScenarioControl",
                         commandName(),
                         description()}
{

}

MoveConstraint::~MoveConstraint()
{

}

MoveConstraint::MoveConstraint(ObjectPath&& scenarioPath,
                               const id_type<ConstraintModel>& id,
                               const TimeValue& date,
                               double height) :
    SerializableCommand{"ScenarioControl",
                        commandName(),
                        description()},
    m_path{std::move(scenarioPath)},
    m_constraint{id},
    m_newHeightPosition{height}
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    auto& scenar = m_path.find<ScenarioModel>();
    auto& cst = scenar.constraint(m_constraint);

    m_oldHeightPosition = cst.heightPercentage();
    m_eventHeight = scenar.event(cst.startEvent()).heightPercentage();
    */
}

void MoveConstraint::update(const ObjectPath& path,
                            const id_type<ConstraintModel>&,
                            const TimeValue& date,
                            double height)
{
    m_newHeightPosition = height;
}

void MoveConstraint::undo()
{
    auto& scenar = m_path.find<ScenarioModel>();
    scenar.constraint(m_constraint).setHeightPercentage(m_oldHeightPosition);
    emit scenar.constraintMoved(m_constraint);
}

void MoveConstraint::redo()
{
    auto& scenar = m_path.find<ScenarioModel>();
    scenar.constraint(m_constraint).setHeightPercentage(m_newHeightPosition);
    emit scenar.constraintMoved(m_constraint);
}

void MoveConstraint::serializeImpl(QDataStream& s) const
{
    s << m_path
      << m_constraint
      << m_oldHeightPosition
      << m_newHeightPosition
      << m_eventHeight;
}

void MoveConstraint::deserializeImpl(QDataStream& s)
{
    QByteArray a;
    s >> a
      >> m_path
      >> m_constraint
      >> m_oldHeightPosition
      >> m_newHeightPosition
      >> m_eventHeight;
}
