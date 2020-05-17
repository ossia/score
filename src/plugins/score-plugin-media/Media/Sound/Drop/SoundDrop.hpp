#pragma once
#include <Media/MediaFileHandle.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/TimeValue.hpp>

namespace Media::Sound
{
struct DroppedAudioFiles
{
  DroppedAudioFiles(const score::DocumentContext& ctx, const QMimeData& mime);

  bool valid() const { return !files.empty() && maxDuration != TimeVal::zero(); }

  TimeVal dropMaxDuration() const;
  TimeVal maxDuration = TimeVal::zero();
  std::vector<std::pair<QString, TimeVal>> files;
};

/**
 * @brief The DropHandler class
 * If something with audio mime type is dropped,
 * then we create a box with an audio file loaded.
 */
class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("bc57983b-c29e-4b12-8afe-9d6ffbcb7a94")

  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;
  std::vector<ProcessDrop>
  drop(const QMimeData& data, const score::DocumentContext& ctx) const noexcept override;
};

}
