#pragma once
#include <Process/FileOperation.hpp>

#include <QString>
#include <QStringList>

#include <score_lib_process_export.h>

#include <vector>

namespace score
{
struct DocumentContext;
}

namespace Process
{

//! Where the leftovers go.
enum class UnusedDisposal
{
  //! Into <project>/Unused/, keeping their place in the tree. Reversible with
  //! a file manager, which is what makes it the default.
  MoveAside,
  //! Deleted outright.
  Delete
};

struct UnusedFilesOptions
{
  /** Look only inside the folders consolidation creates.
   *
   * On by default, and the difference matters: score put those files there and
   * knows what they were for, whereas the rest of a project folder can hold
   * renders, notes, stems, a collaborator's session -- things it has no
   * business proposing to delete.
   */
  bool onlyCollectedFolders = true;

  UnusedDisposal disposal = UnusedDisposal::MoveAside;
};

//! Name of the folder MoveAside puts things in, directly under the project.
SCORE_LIB_PROCESS_EXPORT
QString unusedFolderName() noexcept;

/**
 * @brief Files sitting in the project folder that nothing in the document
 *        points at any more.
 *
 * Consolidating copies media in and never takes it out, so a project that has
 * been worked on accumulates the samples of every idea that was tried and
 * dropped. This finds them.
 *
 * Pure: reads the document and the folder, writes nothing. Entries come back
 * as FileAction::Unused with their size, the largest first, because the reason
 * anyone runs this is disk space.
 */
SCORE_LIB_PROCESS_EXPORT
FileReport
analyzeUnusedFiles(const score::DocumentContext& ctx, const UnusedFilesOptions& opts);

/**
 * @brief Reasons to think twice before acting on that report, in plain words.
 *
 * Empty when there are none. Deciding a file is unused is deciding that
 * nothing score knows about references it, and there are a handful of
 * situations where that is a weaker statement than it sounds -- a broken
 * reference the user is about to relink, a second document sharing the folder.
 * The dialog shows these; the caller cannot get the list by accident.
 */
SCORE_LIB_PROCESS_EXPORT
QStringList
unusedFilesWarnings(const score::DocumentContext& ctx, const FileReport& scan);

/**
 * @brief Move aside or delete exactly the files named.
 *
 * Takes the list rather than recomputing it: what the user ticked in the
 * dialog is what happens, and this function cannot remove something they never
 * saw. Anything outside the project folder, and any score document, is refused
 * whatever the list says.
 */
SCORE_LIB_PROCESS_EXPORT
FileReport removeUnusedFiles(
    const score::DocumentContext& ctx, const std::vector<QString>& absolutePaths,
    const UnusedFilesOptions& opts);
}
