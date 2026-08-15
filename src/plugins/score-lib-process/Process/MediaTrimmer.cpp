#include <Process/MediaTrimmer.hpp>

namespace Process
{
MediaTrimmer::~MediaTrimmer() = default;
MediaTrimmerList::~MediaTrimmerList() = default;

const MediaTrimmer* MediaTrimmerList::find(const QString& absolutePath) const noexcept
{
  for(auto& trimmer : *this)
    if(trimmer.supports(absolutePath))
      return &trimmer;
  return nullptr;
}
}
