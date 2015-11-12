#include "AggregateCommand.hpp"
#include <iscore/presenter/PresenterInterface.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <QApplication>
using namespace iscore;

AggregateCommand::~AggregateCommand()
{

}

void AggregateCommand::undo() const
{
    for(int i = m_cmds.size() - 1; i >= 0; --i)
    {
        m_cmds[i]->undo();
    }
}

void AggregateCommand::redo() const
{
    for(auto& cmd : m_cmds)
    {
        cmd->redo();
    }
}

void AggregateCommand::serializeImpl(QDataStream& s) const
{
    // Meta-data : {{parent name, command name}, command data}
    QList<
            QPair<
                QPair<
            CommandParentFactoryKey,
            CommandFactoryKey
                >,
                QByteArray
            >
    > serializedCommands;

    for(auto& cmd : m_cmds)
    {
        serializedCommands.push_back({{cmd->parentKey(), cmd->key() }, cmd->serialize()});
    }

    s << serializedCommands;
}

void AggregateCommand::deserializeImpl(QDataStream& s)
{
    QList<
            QPair<
                QPair<
                    CommandParentFactoryKey,
                    CommandFactoryKey
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
