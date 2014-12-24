#include "RemoveMultipleElements.hpp"
#include <QApplication>

#include "core/presenter/Presenter.hpp"

using namespace iscore;
using namespace Scenario::Command;

RemoveMultipleElementsCommand::RemoveMultipleElementsCommand(
		QVector<iscore::SerializableCommand*> deletionCommands):
	iscore::SerializableCommand{"ScenarioControl",
								"DeleteMultipleElementsCommand",
								QObject::tr("Group deletion")}
{
	for(auto& cmd : deletionCommands)
	{
		m_serializedCommands.push_back({{cmd->parentName(), cmd->name()}, cmd->serialize()});
	}
}

void RemoveMultipleElementsCommand::undo()
{
	for(auto& cmd_pack : m_serializedCommands)
	{
		// Put this in the ctor as an optimization
		auto presenter = qApp->findChild<iscore::Presenter*>("Presenter");
		auto cmd = presenter->instantiateUndoCommand(cmd_pack.first.first,
													 cmd_pack.first.second,
													 cmd_pack.second);

		cmd->undo();
	}
}

void RemoveMultipleElementsCommand::redo()
{
	for(auto& cmd_pack : m_serializedCommands)
	{
		// Put this in the ctor as an optimization
		auto presenter = qApp->findChild<iscore::Presenter*>("Presenter");
		auto cmd = presenter->instantiateUndoCommand(cmd_pack.first.first,
													 cmd_pack.first.second,
													 cmd_pack.second);

		cmd->redo();
	}
}

int RemoveMultipleElementsCommand::id() const
{
	return 1;
}

bool RemoveMultipleElementsCommand::mergeWith(const QUndoCommand* other)
{
	return false;
}

void RemoveMultipleElementsCommand::serializeImpl(QDataStream& s)
{
	s << m_serializedCommands;
}

void RemoveMultipleElementsCommand::deserializeImpl(QDataStream& s)
{
	s >> m_serializedCommands;
}
