#pragma once

#if defined(SCORE_HAS_BW64_ADM)
#include <Process/Drop/ProcessDropHandler.hpp>

namespace Media::BW64
{

/**
 * @brief Drop handler for BW64 files with ADM metadata
 *
 * When a BW64 file with ADM metadata is dropped:
 * - Creates a Sound process for each ADM audio object (mono channel)
 * - Creates Automation processes for X, Y, Z position curves
 * - Creates Automation processes for Gain and other parameters
 *
 * With Shift held, each ADM object group gets its own interval.
 */
class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("a7e3f2d1-8c4b-4f5e-9a1d-6b2c3e4f5a6b")

public:
  QSet<QString> mimeTypes() const noexcept override;
  QSet<QString> fileExtensions() const noexcept override;

  void dropPath(
      std::vector<ProcessDrop>& drops, const score::FilePath& filename,
      const score::DocumentContext& ctx) const noexcept override;
};

}
#endif // SCORE_HAS_BW64_ADM
