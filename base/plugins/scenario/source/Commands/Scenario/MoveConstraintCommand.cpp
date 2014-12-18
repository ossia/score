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
	m_newHeightPosition{d.relativeY},
	m_newX{d.x}
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());
	auto cst = scenar->constraint(m_constraintId);
	m_oldHeightPosition = cst->heightPercentage();
	m_oldX = cst->startDate();
}

void MoveConstraintCommand::undo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());

	scenar->moveConstraint(m_constraintId, m_oldX, m_oldHeightPosition);
}

void MoveConstraintCommand::redo()
{
	auto scenar = static_cast<ScenarioProcessSharedModel*>(m_scenarioPath.find());

	m_oldHeightPosition = scenar->constraint(m_constraintId)->heightPercentage();

	scenar->moveConstraint(m_constraintId, m_newX, m_newHeightPosition);
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
	s << m_scenarioPath << m_constraintId
	  << m_oldHeightPosition << m_newHeightPosition << m_oldX << m_newX;
}

void MoveConstraintCommand::deserializeImpl(QDataStream& s)
{
	s >> m_scenarioPath >> m_constraintId
	  >> m_oldHeightPosition >> m_newHeightPosition >> m_oldX >> m_newX;
}
