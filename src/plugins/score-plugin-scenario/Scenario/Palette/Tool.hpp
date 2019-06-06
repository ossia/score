#pragma once
#include <QMetaType>

#include <verdigris>
namespace Scenario
{
enum class Tool : int8_t
{
  Disabled,
  Create,
  Select,
  Play,
  MoveSlot,
  Playing
};
}
Q_DECLARE_METATYPE(Scenario::Tool)
W_REGISTER_ARGTYPE(Scenario::Tool)
