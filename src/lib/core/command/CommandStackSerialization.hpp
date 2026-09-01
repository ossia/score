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

  stack.undoable().clear();
  stack.redoable().clear();

  stack.updateStack([&]() {
    stack.setSavedIndex(-1);

    // A command we cannot read stops the history there rather than the load:
    // what precedes it is consistent, what follows would undo against a state
    // we never reached.
    bool ok = true;
    for(const auto& elt : undoStack)
    {
      auto cmd = components.instantiateUndoCommandIfAvailable(elt);

      if(cmd && redo_fun(cmd))
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
        auto cmd = components.instantiateUndoCommandIfAvailable(elt);
        if(!cmd)
          break;

        stack.redoable().push(cmd);
      }
    }
  });
}
}
