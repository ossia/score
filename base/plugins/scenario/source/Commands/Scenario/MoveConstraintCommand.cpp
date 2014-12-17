#include "MoveConstraintCommand.hpp"

#include "Process/ScenarioProcessSharedModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ConstraintData.hpp"

#include <core/application/Application.hpp>
#include <core/view/View.hpp>

#include <QApplication>

using namespace iscore;

// @todo : maybe should we use deplacement value and not absolute ending point.

MoveConstraintCommand::MoveConstraintCommand(ObjectPath &&scenarioPath, ConstraintData d):
	SerializableCommand{"ScenarioControl",
						"MoveEventCommand",
						QObject::tr("Constraint move")},
	m_scenarioPath(std::move(scenarioPath)),
    m_constraintId{d.id},
    m_deltaHeight{d.relativeY},
    m_deltaX{d.x}
{

}

void MoveConstraintCommand::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());

    scenar->moveConstraint(m_constraintId, - m_deltaX, m_oldHeightPosition);
}

void MoveConstraintCommand::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());

	m_oldHeightPosition = scenar->constraint(m_constraintId)->heightPercentage();
    scenar->moveConstraint(m_constraintId, m_deltaX, m_oldHeightPosition + m_deltaHeight);
}

int MoveConstraintCommand::id() const
{
	return 1;
}

bool MoveConstraintCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void MoveConstraintCommand::serializeImpl(QDataStream& s)
{
    s << m_scenarioPath << m_constraintId << m_deltaHeight;
}

void MoveConstraintCommand::deserializeImpl(QDataStream& s)
{
    s >> m_scenarioPath >> m_constraintId >> m_deltaHeight;
}
