#include "AggregateCommand.hpp"
#include <core/interface/presenter/PresenterInterface.hpp>
#include <QApplication>
using namespace iscore;

// TODO Optimize by putting the instantiation of the commands in
// the ctor / after deserialization so that it occurs only once.
void AggregateCommand::undo()
{
    for(int i = m_serializedCommands.size() - 1; i >= 0; --i)
    {
        auto cmd = IPresenter::instantiateUndoCommand(
                       m_serializedCommands[i].first.first,
                       m_serializedCommands[i].first.second,
                       m_serializedCommands[i].second);

        cmd->undo();
    }
}

void AggregateCommand::redo()
{
    for(auto& cmd_pack : m_serializedCommands)
    {
        auto cmd = IPresenter::instantiateUndoCommand(
                       cmd_pack.first.first,
                       cmd_pack.first.second,
                       cmd_pack.second);

        cmd->redo();
    }
}

bool AggregateCommand::mergeWith(const Command* other)
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
