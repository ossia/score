#pragma once

#include <verdigris>
namespace Scenario
{
enum class Tool : uint8_t
{
  Disabled      ,
  Create        ,
  CreateGraph   ,
  CreateSequence,
  Select        ,
  Play          ,
  Playing       ,
};

inline constexpr bool isCreationTool(Tool t) noexcept
{
  switch(t)
  {
    case Tool::Create:
    case Tool::CreateGraph:
    case Tool::CreateSequence:
      return true;
    default:
      return false;
  }
}
}
Q_DECLARE_METATYPE(Scenario::Tool)
W_REGISTER_ARGTYPE(Scenario::Tool)
