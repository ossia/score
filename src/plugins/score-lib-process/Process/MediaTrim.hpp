#pragma once
#include <Process/FileOperation.hpp>

#include <QString>

#include <score_lib_process_export.h>

namespace score
{
struct DocumentContext;
}

namespace Process
{

/**
 * @brief How much of a media file to keep when trimming it down.
 *
 * Every default here leans the same way: it is always better to keep audio
 * nobody needed than to remove audio somebody did. Trimming is the one
 * operation in this family that destroys information, and the applications
 * that got it wrong -- forum threads titled "Minimize Files wrecked my
 * project" are not hard to find -- got it wrong by being eager.
 */
struct TrimOptions
{
  /** Extra media kept on each side of the used region, in seconds.
   *
   * Users routinely keep a clip's file longer than the clip so they can pull
   * a fade out later, and the first complaint about every trim feature is that
   * it took that away. Two seconds is cheap and covers the common edit.
   */
  double handles = 2.0;

  //! Do not touch a file unless trimming actually saves this many bytes.
  qint64 minimumGain = 1024 * 1024;

  //! Do not touch a file whose used region already covers this much of it.
  double maximumUsedFraction = 0.9;

  /** Delete the file the trimmed one replaces.
   *
   * Off by default, and it stays useful off: the untrimmed file simply stops
   * being referenced, so it no longer travels with the project when it is
   * archived, while remaining on disk for undo to fall back on. Turning this
   * on is the only part of score's file handling that can lose data.
   */
  bool removeOriginal = false;
};

/**
 * @brief Plan trimming every media file down to the part the document reads.
 *
 * Side-effect free. Entries come back as FileAction::Trimmed with the size the
 * file would drop to, or FileAction::Skipped with the reason -- which matters
 * more than it sounds: the standing complaint about other implementations is
 * that they silently decline to trim and leave the user guessing.
 *
 * A file used by several processes is planned once, over the union of every
 * region that reads it.
 */
SCORE_LIB_PROCESS_EXPORT
FileReport analyzeMediaTrim(const score::DocumentContext& ctx, const TrimOptions& opts);

/**
 * @brief Perform the trim planned by analyzeMediaTrim.
 *
 * Only files that live inside the project folder are ever touched: a document
 * referencing someone's sample library must not rewrite that library. New
 * files are written beside the originals under fresh names, never over them,
 * and the references are repointed as one undoable command.
 */
SCORE_LIB_PROCESS_EXPORT
FileReport trimProjectMedia(const score::DocumentContext& ctx, const TrimOptions& opts);
}
