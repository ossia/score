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
    auto& scenar = m_path.find<ScenarioModel>();
    auto& cst = scenar.constraint(m_constraint);

    m_oldHeightPosition = cst.heightPercentage();
    m_eventHeight = scenar.event(cst.startEvent()).heightPercentage();

    m_cmd = new MoveEvent{
            ObjectPath{m_path},
            cst.startEvent(),
            date,
            m_eventHeight,
            mode};
}

void MoveConstraint::update(const ObjectPath& path,
                            const id_type<ConstraintModel>&,
                            const TimeValue& date,
                            double height,
                            ExpandMode mode)
{
    m_cmd->update(path, id_type<EventModel>{}, date, m_eventHeight, mode);
    m_newHeightPosition = height;
}

void MoveConstraint::undo()
{
    m_cmd->undo();

    auto& scenar = m_path.find<ScenarioModel>();
    scenar.constraint(m_constraint).setHeightPercentage(m_oldHeightPosition);
}

void MoveConstraint::redo()
{
    m_cmd->redo();

    auto& scenar = m_path.find<ScenarioModel>();
    scenar.constraint(m_constraint).setHeightPercentage(m_newHeightPosition);
}

void MoveConstraint::serializeImpl(QDataStream& s) const
{
    s << m_cmd->serialize()
      << m_path
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

    m_cmd->deserialize(a);
}
