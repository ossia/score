#pragma once
#include <QMetaObject>

#include <score_lib_process_export.h>

#include <verdigris>
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
  Texture,
  Geometry,
};

// Flags for ControlInlet::loadData (the in-memory saveData/loadData blob used by
// undo/redo state capture and port re-matching — NOT document save/load, which
// goes through the value visitor). By default the saved control value is NOT
// applied (the caller re-derives it from its own source: a script's/plugin's
// own defaults, lilv state, loadPreset...). Pass ReloadValue when you want the
// blob's value applied (e.g. LoadPresetCommand::redo).
enum class PortLoadDataFlags
{
  NoFlag = 0,
  ReloadValue = (1 << 0)
};

SCORE_LIB_PROCESS_EXPORT
const score::Brush& portBrush(Process::PortType type);
SCORE_LIB_PROCESS_EXPORT
const score::Brush& labelBrush();
SCORE_LIB_PROCESS_EXPORT
const score::BrushSet& labelBrush(const Process::Port& p);
}

Q_DECLARE_METATYPE(Process::PortType)
W_REGISTER_ARGTYPE(Process::PortType)
