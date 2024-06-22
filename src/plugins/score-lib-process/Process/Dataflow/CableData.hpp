#pragma once
#include <score/model/path/Path.hpp>

#include <score_lib_process_export.h>

namespace Process
{
class Port;
enum class CableType
{
  ImmediateGlutton,
  ImmediateStrict,
  DelayedGlutton,
  DelayedStrict
};

struct SCORE_LIB_PROCESS_EXPORT CableData
{
  CableType type{};
  Path<Process::Port> source, sink;

  SCORE_LIB_PROCESS_EXPORT
  friend bool operator==(const CableData& lhs, const CableData& rhs);
};
}
