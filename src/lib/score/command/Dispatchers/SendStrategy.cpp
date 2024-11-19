#include "SendStrategy.hpp"

#include <score/command/CommandStackFacade.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/command/CommandStack.hpp>

namespace SendStrategy
{

void Simple::send(const score::CommandStackFacade& stack, score::Command* other)
{
  auto trans = stack.context().commandStack.transaction();
  stack.redoAndPush(other);
}

void UndoRedo::send(const score::CommandStackFacade& stack, score::Command* other)
{
  auto trans = stack.context().commandStack.transaction();
  other->undo(stack.context());
  stack.redoAndPush(other);
}

void Quiet::send(const score::CommandStackFacade& stack, score::Command* other)
{
  stack.push(other);
}

}
namespace RedoStrategy
{
void Redo::redo(const score::DocumentContext& ctx, score::Command& cmd)
{
  auto trans = ctx.commandStack.transaction();
  cmd.redo(ctx);
}

void Quiet::redo(const score::DocumentContext& ctx, score::Command& cmd) { }

}
