#include <QByteArray>
#include <QDataStream>
#include <QList>
#include <QPair>
#include <QStack>
#include <QtGlobal>
#include <iscore/serialization/DataStreamVisitor.hpp>

#include "CommandStack.hpp"
#include <iscore/application/ApplicationComponents.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/customfactory/StringFactoryKeySerialization.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
void Visitor<Reader<DataStream>>::readFrom(const iscore::CommandStack& stack)
{
  std::vector<iscore::CommandData> undoStack, redoStack;
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
