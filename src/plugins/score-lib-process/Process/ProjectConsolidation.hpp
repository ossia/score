#pragma once
#include <Process/ExternalFiles.hpp>

#include <score/tools/ProjectFiles.hpp>

#include <QString>

#include <score_lib_process_export.h>

#include <vector>

namespace score
{
struct DocumentContext;
}

namespace Process
{

//! What consolidation decided to do about one referenced file.
enum class ConsolidationAction
{
  Collect,       //!< Copy (or link) it into the project folder and repoint it.
  AlreadyThere,  //!< Already inside the project folder: only the reference changes.
  KeptInLibrary, //!< Resolves through the user library and was left there.
  Missing,       //!< The file does not exist: nothing to collect, reported instead.
  Unsupported,   //!< A folder, or a reference score cannot relocate.
  Failed         //!< Collecting it was attempted and did not work.
};

struct ConsolidationEntry
{
  //! Reference as stored in the document.
  QString storedPath;
  //! Absolute path the reference resolves to right now (empty when unresolved).
  QString sourcePath;
  //! Absolute path it will be / was collected to (empty when nothing is copied).
  QString destinationPath;
  //! What the reference becomes in the document ("<PROJECT>:Audio/kick.wav").
  QString newStoredPath;

  score::FileKind kind{};
  FileUsage usage{};
  ConsolidationAction action{};
  QString owner;
  //! Size of the source in bytes; 0 when unknown.
  qint64 size{};
  //! Bytes actually have to move. False when the destination already holds
  //! the same content, which is what makes a second run free.
  bool copyNeeded{};
  //! Filled for ConsolidationAction::Failed.
  QString error;
};

/**
 * @brief Result of analysing — or of running — a project consolidation.
 */
struct SCORE_LIB_PROCESS_EXPORT ConsolidationReport
{
  QString projectFolder;
  std::vector<ConsolidationEntry> entries;

  //! Bytes that will be / were copied. Links do not count.
  qint64 bytesToCopy() const noexcept;
  int count(ConsolidationAction) const noexcept;
  //! Files referenced by the document that could not be found anywhere.
  std::vector<const ConsolidationEntry*> missing() const noexcept;
  bool empty() const noexcept { return entries.empty(); }
};

/**
 * @brief Plan the consolidation of a document into its own folder.
 *
 * Pure: reads the document and the filesystem, writes nothing. The result is
 * what the UI shows the user before committing.
 *
 * `projectFolder` defaults to the folder holding the document. Passing a
 * different one is how "save a copy into a new project folder" works: the
 * references still resolve against the document's current location, while the
 * destinations are computed for the new one.
 */
SCORE_LIB_PROCESS_EXPORT
ConsolidationReport analyzeProjectFiles(
    const score::DocumentContext& ctx, const score::ConsolidateOptions& opts,
    QString projectFolder = {});

/**
 * @brief Perform the consolidation planned by analyzeProjectFiles.
 *
 * Copies the files, then rewrites the document in a single undoable command
 * pushed on `ctx`'s command stack. Originals are never moved or deleted, and
 * an existing destination is never overwritten.
 *
 * The returned report describes what actually happened, entry per entry: a
 * copy that failed comes back as ConsolidationAction::Failed with its error,
 * and its reference is left untouched.
 */
SCORE_LIB_PROCESS_EXPORT
ConsolidationReport consolidateProjectFiles(
    const score::DocumentContext& ctx, const score::ConsolidateOptions& opts,
    QString projectFolder = {});

//! How many references the document resolves through its own folder.
//! These are exactly the ones that stop working when the document is saved
//! somewhere else without its media.
SCORE_LIB_PROCESS_EXPORT
int countProjectRelativeFiles(const score::DocumentContext& ctx);

/**
 * @brief Turn every project-relative reference into the absolute file it
 *        currently points at.
 *
 * The counterpart of consolidating when a document is saved into another
 * folder: rather than let "<PROJECT>:Audio/kick.wav" quietly start meaning a
 * file that is not there, pin it to the one that is. The result is a document
 * that still plays, at the cost of no longer being self-contained.
 *
 * Returns the number of references that were re-anchored. The change goes on
 * the command stack like any other.
 */
SCORE_LIB_PROCESS_EXPORT
int reanchorProjectFiles(const score::DocumentContext& ctx);
}
