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

    // A command we cannot read stops the history there rather than the load.
    // The stack can name plug-ins this build does not have -- a document
    // travels, and in a session it comes from another machine entirely -- and
    // losing the ability to undo past that point is a far smaller thing than
    // failing to open the document at all. Everything before it is still
    // consistent, so it is kept; everything after it would be undone against a
    // state we could not reach, so it is dropped.
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
