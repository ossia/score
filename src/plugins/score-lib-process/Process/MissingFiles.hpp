#pragma once
#include <Process/FileOperation.hpp>

#include <QHash>
#include <QMultiHash>
#include <QString>

#include <score_lib_process_export.h>

#include <vector>

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
class SCORE_LIB_PROCESS_EXPORT FileIndex
{
public:
  /** Walk `folder` recursively.
   *
   * `maxFiles` is a guard against someone pointing this at their home folder
   * or at "/": the scan stops there rather than freezing the application, and
   * truncated() then says the answer may be incomplete.
   */
  void scan(const QString& folder, int maxFiles = 250000);

  //! Absolute paths whose file name matches, case-insensitively, best first.
  //! `size` (when > 0) promotes exact size matches, which is the difference
  //! between "a file called kick.wav" and "this kick.wav".
  std::vector<QString> candidates(const QString& missingPath, qint64 size = -1) const;

  int fileCount() const noexcept { return int(m_byName.size()); }
  bool truncated() const noexcept { return m_truncated; }
  const QString& root() const noexcept { return m_root; }

private:
  QString m_root;
  //! lower-cased file name -> absolute path
  QMultiHash<QString, QString> m_byName;
  bool m_truncated{};
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
