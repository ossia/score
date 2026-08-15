#pragma once
#include <Process/MediaTrimmer.hpp>

#include <score_plugin_media_export.h>

namespace Media
{

/**
 * @brief Writes the used part of an audio file into a new WAV.
 *
 * Decodes at the source's own rate -- resampling here would change the audio,
 * and trimming must not -- then writes 32-bit float, which is what score
 * decodes to. That can make a compressed source grow, which is exactly why the
 * caller keeps the result only when it is actually smaller.
 */
class SCORE_PLUGIN_MEDIA_EXPORT AudioTrimmer final : public Process::MediaTrimmer
{
  SCORE_CONCRETE("a35a3f4b-6c2e-4f3a-8f5d-93a1c2b4e7d6")
public:
  ~AudioTrimmer() override;

  bool supports(const QString& absolutePath) const noexcept override;
  QString outputExtension() const noexcept override;
  double duration(const QString& absolutePath) const override;
  qint64 estimatedSize(const QString& absolutePath, Process::MediaRange kept)
      const override;
  QString trim(
      const QString& source, const QString& destination,
      Process::MediaRange range) const override;
};
}
