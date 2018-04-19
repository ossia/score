// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AggregateCommand.hpp"

#include <QByteArray>
#include <QDataStream>
#include <QList>
#include <QPair>
#include <QtGlobal>
#include <boost/concept/usage.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <score/application/ApplicationComponents.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/command/Command.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace score
{
AggregateCommand::~AggregateCommand()
{
  qDeleteAll(m_cmds);
  m_cmds.clear();
}

void AggregateCommand::undo(const score::DocumentContext& ctx) const
{
  for (const auto& cmd : boost::adaptors::reverse(m_cmds))
  {
    cmd->undo(ctx);
  }
}

void AggregateCommand::redo(const score::DocumentContext& ctx) const
{
  for (const auto& cmd : m_cmds)
  {
    cmd->redo(ctx);
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

  DataStreamReader reader{s.stream.device()};
  reader.readFrom(serializedCommands);
}

void AggregateCommand::deserializeImpl(DataStreamOutput& s)
{
  std::vector<CommandData> serializedCommands;
  DataStreamWriter writer{s.stream.device()};
  writer.writeTo(serializedCommands);

  const auto& context = score::AppContext();
  for (const auto& cmd_pack : serializedCommands)
  {
    auto cmd = context.instantiateUndoCommand(cmd_pack);
    m_cmds.push_back(cmd);
  }
}
}
