#pragma once
#include <iscore/command/CommandStackFacade.hpp>

namespace SendStrategy
{
struct Simple
{
  static void send(
      const iscore::CommandStackFacade& stack,
      iscore::Command* other)
  {
    stack.redoAndPush(other);
  }
};

struct Quiet
{
  static void send(
      const iscore::CommandStackFacade& stack,
      iscore::Command* other)
  {
    stack.push(other);
  }
};

struct UndoRedo
{
  static void send(
      const iscore::CommandStackFacade& stack,
      iscore::Command* other)
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
  static void redo(
      const iscore::DocumentContext& ctx,
      iscore::Command& cmd)
  {
    cmd.redo(ctx);
  }
};

struct Quiet
{
  static void redo(
      const iscore::DocumentContext& ctx,
      iscore::Command& cmd)
  {
  }
};
}
