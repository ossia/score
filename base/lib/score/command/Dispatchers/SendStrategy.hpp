#pragma once
#include <score/command/CommandStackFacade.hpp>

namespace SendStrategy
{
struct Simple
{
  static void
  send(const score::CommandStackFacade& stack, score::Command* other)
  {
    stack.redoAndPush(other);
  }
};

struct Quiet
{
  static void
  send(const score::CommandStackFacade& stack, score::Command* other)
  {
    stack.push(other);
  }
};

struct UndoRedo
{
  static void
  send(const score::CommandStackFacade& stack, score::Command* other)
  {
    other->undo(stack.context());
    stack.redoAndPush(other);
  }
};
}
namespace RedoStrategy
{
struct Redo
{
  static void redo(const score::DocumentContext& ctx, score::Command& cmd)
  {
    cmd.redo(ctx);
  }
};

struct Quiet
{
  static void redo(const score::DocumentContext& ctx, score::Command& cmd)
  {
  }
};
}
