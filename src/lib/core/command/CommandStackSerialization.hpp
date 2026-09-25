#pragma once
#include <score/application/ApplicationComponents.hpp>
#include <score/command/CommandData.hpp>
#include <score/plugins/StringFactoryKeySerialization.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

#include <core/command/CommandStack.hpp>
namespace score
{
template <typename RedoFun>
void loadCommandStack(
    const score::ApplicationComponents& components, DataStreamWriter& writer,
    score::CommandStack& stack, RedoFun redo_fun)
{
  std::vector<score::CommandData> undoStack, redoStack;
  writer.writeTo(undoStack);
  writer.writeTo(redoStack);

  writer.checkDelimiter();

  // The stack owns its commands (see ~CommandStack).
  qDeleteAll(stack.undoable());
  stack.undoable().clear();
  qDeleteAll(stack.redoable());
  stack.redoable().clear();

  stack.updateStack([&]() {
    stack.setSavedIndex(-1);

    bool ok = true;
    for(const auto& elt : undoStack)
    {
      auto cmd = components.instantiateUndoCommand(elt);

      if(redo_fun(cmd))
      {
        stack.undoable().push(cmd);
      }
      else
      {
        ok = false;
        break;
      }
    }

    if(ok)
    {
      for(const auto& elt : redoStack)
      {
        auto cmd = components.instantiateUndoCommand(elt);

        stack.redoable().push(cmd);
      }
    }
  });
}
}
