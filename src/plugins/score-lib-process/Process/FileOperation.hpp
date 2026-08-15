#pragma once
#include <Process/ExternalFiles.hpp>

#include <score/tools/ProjectFiles.hpp>

#include <QString>

#include <score_lib_process_export.h>

#include <functional>
#include <vector>

namespace score
{
struct DocumentContext;
}

namespace Process
{

/**
 * @brief What an operation decided about one file reference.
 *
 * Shared by every operation that walks a document's files -- consolidating,
 * re-anchoring, scanning for missing media, relinking, trimming, archiving --
 * so that they all produce a report the same widgets can display and the same
 * code can summarize.
 */
enum class FileAction
{
  Unchanged,     //!< Seen, nothing to do.
  Collect,       //!< Copied into the project folder and repointed.
  AlreadyThere,  //!< Already inside the project; only the reference changed.
  KeptInLibrary, //!< Resolves through the user library and was left there.
  Relinked,      //!< Was missing, found elsewhere, repointed.
  Trimmed,       //!< Rewritten down to the part the document actually reads.
  Missing,       //!< Not found. Left untouched.
  Unsupported,   //!< A folder, or something score has no way to relocate.
  Skipped,       //!< In scope but declined; `note` says why.
  Failed         //!< Attempted and did not work; `note` says why.
};

SCORE_LIB_PROCESS_EXPORT
QString toString(FileAction) noexcept;

struct FileEntry
{
  //! Reference exactly as the document stores it.
  QString storedPath;
  //! Absolute path it resolves to right now (empty when unresolved).
  QString sourcePath;
  //! Absolute path it was / will be written to (empty when nothing is written).
  QString destinationPath;
  //! What the reference becomes in the document.
  QString newStoredPath;

  score::FileKind kind{};
  FileUsage usage{};
  FileAction action{FileAction::Unchanged};
  //! Human-readable name of the object holding the reference.
  QString owner;

  //! Size of the source in bytes, 0 when unknown.
  qint64 size{};
  //! Size of what was written, when it differs from the source (trimming).
  qint64 newSize{};
  //! Bytes actually have to move. False when the destination already holds
  //! the same content, which is what makes a second run free.
  bool copyNeeded{};

  //! Why the operation skipped or failed, shown to the user as-is.
  QString note;
};

struct SCORE_LIB_PROCESS_EXPORT FileReport
{
  QString projectFolder;
  std::vector<FileEntry> entries;

  bool empty() const noexcept { return entries.empty(); }
  int count(FileAction) const noexcept;
  std::vector<const FileEntry*> with(FileAction) const noexcept;

  //! Bytes that will be / were copied. Links and reuses do not count.
  qint64 bytesToCopy() const noexcept;
  //! Bytes trimming removed. Negative would mean it made things worse, which
  //! is why the trimmer refuses those.
  qint64 bytesSaved() const noexcept;
};

/**
 * @brief Decides what happens to one reference.
 *
 * Fills `entry` (action, destination, sizes, note) and returns the path the
 * reference must be rewritten to, or an empty string to leave it alone.
 */
using FilePolicy = std::function<QString(const ExternalFileRef&, FileEntry&)>;

/**
 * @brief Run `policy` over every external file reference of a document.
 *
 * This is the single traversal every file operation in score goes through.
 * `dryRun` suppresses the rewrites without changing the report by one field,
 * so "show me what would happen" and "do it" cannot describe different things.
 * Otherwise the rewrites are committed as one undoable command.
 */
SCORE_LIB_PROCESS_EXPORT
FileReport runFileOperation(
    const score::DocumentContext& ctx, const FilePolicy& policy, bool dryRun);
}
