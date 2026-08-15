#pragma once
#include <Process/ExternalFiles.hpp>

#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <QString>

#include <score_lib_process_export.h>

namespace Process
{

/**
 * @brief Writes the used part of a media file into a new one.
 *
 * The trimming operation itself lives in whichever plug-in knows how to read
 * and write that kind of media -- audio in score-plugin-media, and video in
 * the graphics plug-in if anyone ever wants it -- while the decision of what
 * to trim, and all the safety around it, stays in one place.
 *
 * Implementations must never write over their source and never touch anything
 * but `destination`.
 */
class SCORE_LIB_PROCESS_EXPORT MediaTrimmer : public score::InterfaceBase
{
  SCORE_INTERFACE(MediaTrimmer, "3ee2a1c8-4d6b-4a51-9f30-1f0d3b7d6c92")
public:
  ~MediaTrimmer() override;

  //! Can this trimmer read that file?
  virtual bool supports(const QString& absolutePath) const noexcept = 0;

  //! The extension the trimmed file must take, without the dot.
  virtual QString outputExtension() const noexcept = 0;

  //! Duration of the file in seconds, or 0 if it cannot be read.
  virtual double duration(const QString& absolutePath) const = 0;

  /** Size the trimmed file would take, so the plan shown to the user is the
   * one that happens.
   *
   * Scaling the source's size by the kept fraction is not good enough: a
   * trimmer that re-encodes -- and the audio one writes 32-bit float -- can
   * hand back a bigger file from a smaller region, and a plan promising space
   * that never appears is exactly the complaint other implementations get.
   */
  virtual qint64 estimatedSize(const QString& absolutePath, MediaRange kept) const = 0;

  /** Write `range` of `source` into `destination`.
   *
   * `range` is in seconds of the source's own timeline and has already been
   * clamped to it. Returns an empty string on success, the reason otherwise.
   */
  virtual QString
  trim(const QString& source, const QString& destination, MediaRange range) const = 0;
};

class SCORE_LIB_PROCESS_EXPORT MediaTrimmerList final
    : public score::InterfaceList<MediaTrimmer>
{
public:
  ~MediaTrimmerList();

  //! The first trimmer that accepts this file, or nullptr.
  const MediaTrimmer* find(const QString& absolutePath) const noexcept;
};
}
