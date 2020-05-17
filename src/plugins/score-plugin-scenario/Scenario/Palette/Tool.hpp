#pragma once

#include <verdigris>
namespace Scenario
{
enum class Tool : int8_t
{
  Disabled,
  Create,
  CreateGraph,
  Select,
  Play,
  MoveSlot,
  Playing
};
}
Q_DECLARE_METATYPE(Scenario::Tool)
W_REGISTER_ARGTYPE(Scenario::Tool)
