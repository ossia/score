#include "AggregateCommand.hpp"
#include <iscore/presenter/PresenterInterface.hpp>
#include <QApplication>
using namespace iscore;

// TODO Optimize by putting the instantiation of the commands in
// the ctor / after deserialization so that it occurs only once.

// More generally, commands should take real values as arguments (not objectpaths, etc.)
void AggregateCommand::undo()
{
    for(int i = m_cmds.size() - 1; i >= 0; --i)
    {
        m_cmds[i]->undo();
    }
}

void AggregateCommand::redo()
{
    for(auto& cmd : m_cmds)
    {
        cmd->redo();
    }
}

bool AggregateCommand::mergeWith(const Command* other)
{
    return false;
}

void AggregateCommand::serializeImpl(QDataStream& s) const
{
    // Meta-data : {{parent name, command name}, command data}
    QList<
            QPair<
                QPair<
                    QString,
                    QString
                >,
                QByteArray
            >
    > serializedCommands;

    for(auto& cmd : m_cmds)
    {
        serializedCommands.push_back({{cmd->parentName(), cmd->name() }, cmd->serialize()});
    }

    s << serializedCommands;
}

void AggregateCommand::deserializeImpl(QDataStream& s)
{
    QList<
            QPair<
                QPair<
                    QString,
                    QString
                >,
                QByteArray
            >
    > serializedCommands;
    s >> serializedCommands;

    for(auto& cmd_pack : serializedCommands)
    {
        auto cmd = IPresenter::instantiateUndoCommand(
                       cmd_pack.first.first,
                       cmd_pack.first.second,
                       cmd_pack.second);
        m_cmds.push_back(cmd);
    }
}
