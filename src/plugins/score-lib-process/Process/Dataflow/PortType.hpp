#pragma once
#include <score_lib_process_export.h>
namespace score
{
struct Brush;
struct BrushSet;
}
namespace Process
{
class Port;

enum class PortType
{
  Message,
  Audio,
  Midi,
  Texture
};

SCORE_LIB_PROCESS_EXPORT
const score::Brush& portBrush(Process::PortType type);
SCORE_LIB_PROCESS_EXPORT
const score::Brush& labelBrush();
SCORE_LIB_PROCESS_EXPORT
const score::BrushSet& labelBrush(const Process::Port& p);
}
