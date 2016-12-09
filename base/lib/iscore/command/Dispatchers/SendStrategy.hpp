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
    other->undo();
    stack.redoAndPush(other);
  }
};
}
namespace RedoStrategy
{
struct Redo
{
  static void redo(iscore::Command& cmd)
  {
    cmd.redo();
  }
};

struct Quiet
{
  static void redo(iscore::Command& cmd)
  {
  }
};
}
