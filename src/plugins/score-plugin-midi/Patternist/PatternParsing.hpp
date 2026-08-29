#pragma once
#include <score_plugin_midi_export.h>
#include <Patternist/PatternModel.hpp>

#include <vector>

class QMimeData;
class QByteArray;

namespace Patternist
{
SCORE_PLUGIN_MIDI_EXPORT
std::vector<Pattern> parsePatterns(const QByteArray& data) noexcept;
std::vector<std::vector<Pattern>> parsePatternFiles(const QMimeData& mime) noexcept;
}
