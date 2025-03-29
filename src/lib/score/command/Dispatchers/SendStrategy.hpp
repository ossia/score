#pragma once
#include <score_lib_base_export.h>
namespace score
{
class CommandStackFacade;
class Command;
struct DocumentContext;
}

namespace SendStrategy
{
struct SCORE_LIB_BASE_EXPORT Simple
{
  static void send(const score::CommandStackFacade& stack, score::Command* other);
};

struct SCORE_LIB_BASE_EXPORT Quiet
{
  static void send(const score::CommandStackFacade& stack, score::Command* other);
};

struct SCORE_LIB_BASE_EXPORT UndoRedo
{
  static void send(const score::CommandStackFacade& stack, score::Command* other);
};
}
namespace RedoStrategy
{
struct SCORE_LIB_BASE_EXPORT Redo
{
  static void redo(const score::DocumentContext& ctx, score::Command& cmd);
};

struct SCORE_LIB_BASE_EXPORT Quiet
{
  static void redo(const score::DocumentContext& ctx, score::Command& cmd);
};
}
