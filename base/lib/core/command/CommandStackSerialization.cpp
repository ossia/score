// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CommandStack.hpp"

#include <QByteArray>
#include <QDataStream>
#include <QList>
#include <QPair>
#include <QStack>
#include <QtGlobal>
#include <score/application/ApplicationComponents.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/command/Command.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/plugins/customfactory/StringFactoryKeySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
void DataStreamReader::read(const score::CommandStack& stack)
{
  std::vector<score::CommandData> undoStack, redoStack;
  for (const auto& cmd : stack.undoable())
  {
    undoStack.emplace_back(*cmd);
  }
  readFrom(undoStack);

  for (const auto& cmd : stack.redoable())
  {
    redoStack.emplace_back(*cmd);
  }
  readFrom(redoStack);

  insertDelimiter();
}
