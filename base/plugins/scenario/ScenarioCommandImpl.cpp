#include "ScenarioCommandImpl.hpp"
#include "ScenarioCommand.hpp"
#include <core/presenter/command/Command.hpp>
#include <QApplication>
using namespace iscore;
ScenarioCommandImpl::ScenarioCommandImpl(QPointF pos):
	Command{QString("ScenarioCommand"),
			QString("ScenarioCommandImpl"),
			QString("Increment process")}
{
	m_position = pos;
}

QByteArray ScenarioCommandImpl::serialize()
{
	auto arr = Command::serialize();
	{
		QDataStream s(&arr, QIODevice::Append);
		s.setVersion(QDataStream::Qt_5_3);

		s << 42;
	}

	return arr;
}

void ScenarioCommandImpl::deserialize(QByteArray arr)
{
	QBuffer buf(&arr, nullptr);
	buf.open(QIODevice::ReadOnly);
	cmd_deserialize(&buf);

	QDataStream stream(&buf);
	int test;
	stream >> test;
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

