#pragma once

#include <Media/AudioArray.hpp>
#include <Media/AudioDecoder.hpp>

namespace Media
{
class SndfileDecoder
{
public:
  SndfileDecoder();

  void decode(const QString& path, audio_handle hdl);
  static std::optional<AudioInfo> do_probe(const QString& path);

  int32_t fileSampleRate{};
  int32_t convertedSampleRate{};
  int32_t channels{};
  std::size_t decoded{};
};
}
