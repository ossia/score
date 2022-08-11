#pragma once
#include <Patternist/PatternModel.hpp>

#include <vector>

class QMimeData;
class QByteArray;

namespace Patternist
{
Pattern parsePattern(const QByteArray& data) noexcept;
std::vector<Pattern> parsePatternFiles(const QMimeData& mime) noexcept;
}
