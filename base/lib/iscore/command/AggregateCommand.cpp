#include "AggregateCommand.hpp"
#include <core/application/ApplicationComponents.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <QApplication>
#include <boost/range/adaptor/reversed.hpp>
using namespace iscore;

AggregateCommand::~AggregateCommand()
{

}

void AggregateCommand::undo() const
{
    for (auto cmd : boost::adaptors::reverse(m_cmds))
    {
        cmd->undo();
    }
}

void AggregateCommand::redo() const
{
    for(auto cmd : m_cmds)
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
        auto cmd = context.components.instantiateUndoCommand(
                       cmd_pack.first.first,
                       cmd_pack.first.second,
                       cmd_pack.second);
        m_cmds.push_back(cmd);
    }
}
