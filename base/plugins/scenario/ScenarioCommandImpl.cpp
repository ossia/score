#include "ScenarioCommandImpl.hpp"
#include "ScenarioCommand.hpp"
#include <core/presenter/command/Command.hpp>
#include <QApplication>
using namespace iscore;
ScenarioCommandImpl::ScenarioCommandImpl(QPointF pos):
	SerializableCommand{QString("ScenarioCommand"),
			QString("ScenarioCommandImpl"),
			QString("Increment process")}
{
	m_position = pos;
}

void ScenarioCommandImpl::serializeImpl(QDataStream&)
{
}

void ScenarioCommandImpl::deserializeImpl(QDataStream&)
{
}

void ScenarioCommandImpl::undo()
{
	qDebug(Q_FUNC_INFO);
	auto target = qApp->findChild<ScenarioCommand*>(parentName());
	if(target)
		target->decrementProcesses();
}

void ScenarioCommandImpl::redo()
{
	qDebug(Q_FUNC_INFO);
	auto target = qApp->findChild<ScenarioCommand*>(parentName());
	if(target)
		target->incrementProcesses();
		target->emitCreateTimeEvent(m_position);
}

