#include <QByteArray>
#include <QDataStream>
#include <QList>
#include <QPair>
#include <QtGlobal>
#include <boost/concept/usage.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <iscore/application/ApplicationComponents.hpp>

#include "AggregateCommand.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>

namespace iscore
{
AggregateCommand::~AggregateCommand()
{
  qDeleteAll(m_cmds);
  m_cmds.clear();
}

void AggregateCommand::undo() const
{
  for (const auto& cmd : boost::adaptors::reverse(m_cmds))
  {
    cmd->undo();
  }
}

void AggregateCommand::redo() const
{
  for (const auto& cmd : m_cmds)
  {
    cmd->redo();
  }
}

void AggregateCommand::addCommand(Command* cmd)
{
    m_cmds.push_back(command_ptr{cmd});
}

int AggregateCommand::count() const
{
    return m_cmds.size();
}

void AggregateCommand::serializeImpl(DataStreamInput& s) const
{
    std::vector<CommandData> serializedCommands;
  serializedCommands.reserve(m_cmds.size());

  for (auto& cmd : m_cmds)
  {
    serializedCommands.emplace_back(*cmd);
  }

  Visitor<Reader<DataStream>> reader{s.stream.device()};
  reader.readFrom(serializedCommands);
}

void AggregateCommand::deserializeImpl(DataStreamOutput& s)
{
  std::vector<CommandData> serializedCommands;
  Visitor<Writer<DataStream>> writer{s.stream.device()};
  writer.writeTo(serializedCommands);

  for (const auto& cmd_pack : serializedCommands)
  {
    auto cmd = context.instantiateUndoCommand(cmd_pack);
    m_cmds.push_back(cmd);
  }
}
}
