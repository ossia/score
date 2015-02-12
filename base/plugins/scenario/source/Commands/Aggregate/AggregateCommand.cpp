#include "AggregateCommand.hpp"
#include <core/presenter/Presenter.hpp>
#include <QApplication>
using namespace iscore;
using namespace Scenario::Command;


void AggregateCommand::undo()
{
	auto presenter = qApp->findChild<iscore::Presenter*>("Presenter");


	for(int i = m_serializedCommands.size() - 1; i >= 0; --i)
	{
		auto cmd = presenter->instantiateUndoCommand(m_serializedCommands[i].first.first,
													 m_serializedCommands[i].first.second,
													 m_serializedCommands[i].second);

		cmd->undo();
	}
}

void AggregateCommand::redo()
{
	auto presenter = qApp->findChild<iscore::Presenter*>("Presenter");
	for(auto& cmd_pack : m_serializedCommands)
	{
		auto cmd = presenter->instantiateUndoCommand(cmd_pack.first.first,
													 cmd_pack.first.second,
													 cmd_pack.second);

		cmd->redo();
	}
}


int AggregateCommand::id() const
{
	return -1;
}

bool AggregateCommand::mergeWith(const QUndoCommand *other)
{
	return false;
}

void AggregateCommand::serializeImpl(QDataStream& s) const
{
	s << m_serializedCommands;
}

void AggregateCommand::deserializeImpl(QDataStream& s)
{
	s >> m_serializedCommands;
}
