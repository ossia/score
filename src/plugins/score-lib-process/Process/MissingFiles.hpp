#pragma once
#include <Process/FileOperation.hpp>

#include <QHash>
#include <QMultiHash>
#include <QString>

#include <score_lib_process_export.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

class QObject;

namespace score
{
struct DocumentContext;
}

namespace Process
{

/**
 * @brief Everything the document references and cannot find right now.
 *
 * Cheap (one stat per reference) and side-effect free, which is what lets it
 * run on every document load. Entries come back with FileAction::Missing;
 * references that resolve are reported as Unchanged so the caller can also
 * answer "how many files does this project use".
 */
SCORE_LIB_PROCESS_EXPORT
FileReport scanMissingFiles(const score::DocumentContext& ctx);

/**
 * @brief An index of the files under a folder, used to relink by name.
 *
 * Name-only matching is what every application does and the first thing users
 * complain about, so this also keeps sizes: when several files share a name,
 * the one whose size matches the reference wins, and the rest are still
 * offered rather than hidden.
 */
class FileScan;

class SCORE_LIB_PROCESS_EXPORT FileIndex
{
public:
  //! Walk `folder` recursively and exhaustively. A cancelled `progress` stops
  //! the walk and leaves the index holding what it had reached so far.
  void scan(const QString& folder, FileScan* progress = nullptr);

  //! Absolute paths whose file name matches, case-insensitively, best first.
  //! `size`, when > 0, promotes exact size matches.
  std::vector<QString> candidates(const QString& missingPath, qint64 size = -1) const;

  int fileCount() const noexcept { return int(m_byName.size()); }
  const QString& root() const noexcept { return m_root; }

private:
  QString m_root;
  //! lower-cased file name -> absolute path
  QMultiHash<QString, QString> m_byName;
};

/**
 * @brief A folder walk running on the task pool, and the handle to it.
 *
 * The only state shared between the two threads: the GUI samples the progress,
 * the walk reads the cancel flag. Held by shared_ptr on both sides so the state
 * outlives a window closed mid-search.
 */
class SCORE_LIB_PROCESS_EXPORT FileScan
{
public:
  //! Called on `context`'s thread when the walk ends, cancelled or not.
  using OnFinished = std::function<void(FileIndex, bool cancelled)>;

  explicit FileScan(QString folder);

  //! Start walking `folder` on a task-pool thread. `onFinished` does not run
  //! if `context` died first.
  static std::shared_ptr<FileScan>
  start(const QString& folder, QObject* context, OnFinished onFinished);

  void cancel() noexcept { m_cancelled.store(true, std::memory_order_relaxed); }
  bool cancelled() const noexcept
  {
    return m_cancelled.load(std::memory_order_relaxed);
  }

  int filesSeen() const noexcept { return m_filesSeen.load(std::memory_order_relaxed); }
  //! The folder the walk is inside right now.
  QString currentFolder() const;

  const QString& root() const noexcept { return m_root; }

private:
  friend class FileIndex;
  void fileSeen() noexcept { m_filesSeen.fetch_add(1, std::memory_order_relaxed); }
  void setCurrentFolder(const QString& folder);

  QString m_root;
  mutable std::mutex m_mutex;
  QString m_currentFolder;
  std::atomic_int m_filesSeen{};
  std::atomic_bool m_cancelled{};
};

/**
 * @brief Repoint references at the files they were found at.
 *
 * `chosen` maps a reference's stored path to the absolute file it should now
 * use; a stored path used by several processes relinks all of them at once,
 * which is the common case when a whole folder went missing.
 *
 * The new reference is made project- or library-relative when it can be, so
 * relinking leaves the document more portable than it found it. Everything is
 * committed as one undoable command.
 */
SCORE_LIB_PROCESS_EXPORT
FileReport relinkFiles(
    const score::DocumentContext& ctx, const QHash<QString, QString>& chosen);
}
