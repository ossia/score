#pragma once
#include <Patternist/PatternModel.hpp>

#include <vector>

class QMimeData;
class QByteArray;

namespace Patternist
{
std::vector<Pattern> parsePatterns(const QByteArray& data) noexcept;
std::vector<std::vector<Pattern>> parsePatternFiles(const QMimeData& mime) noexcept;
}
