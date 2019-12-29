#pragma once
#include <score_lib_process_export.h>
namespace score
{
struct Brush;
}
namespace Process
{

enum class PortType
{
  Message,
  Audio,
  Midi,
  Texture
};

SCORE_LIB_PROCESS_EXPORT
const score::Brush& portBrush(Process::PortType type);
}
