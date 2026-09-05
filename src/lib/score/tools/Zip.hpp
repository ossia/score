#pragma once
#include <QByteArray>
#include <QString>

#include <score_lib_base_export.h>

#include <functional>
#include <optional>
#include <vector>

namespace score
{

//! One file to put in an archive.
struct ZipEntry
{
  //! Absolute path of the file to read.
  QString sourceFile;
  //! Path it takes inside the archive, always with forward slashes.
  QString nameInArchive;
};

//! Called with (files written, total). Return false to abort the archive.
using ZipProgress = std::function<bool(int, int)>;

/**
 * @brief Write `entries` into a zip archive at `destination`.
 *
 * Aborting or failing leaves no half-written archive behind: the archive is
 * built next to the destination and only moved into place once complete.
 *
 * `level` is 0 (store) to 9 (smallest). Media is already compressed, so the
 * default is deliberately low -- spending minutes to shave a percent off a
 * folder of video is not a trade anyone wants.
 */
SCORE_LIB_BASE_EXPORT
bool writeZipArchive(
    const QString& destination, const std::vector<ZipEntry>& entries, int level,
    QString& error, const ZipProgress& progress = {});

//! What a project archive holds, read from the archive's directory only.
struct ZipArchiveSummary
{
  //! Path of the .score inside the archive, e.g. "foo.score" or "foo/foo.score"
  QString scoreFile;
  int files{};
  quint64 uncompressedSize{};
};

/**
 * @brief Look at a zip's table of contents without inflating anything.
 *
 * Returns nothing if the file is not a zip or holds no .score / .scorejson at
 * its root or one folder down. When several qualify, the shallowest wins,
 * then the one named like the archive.
 */
SCORE_LIB_BASE_EXPORT
std::optional<ZipArchiveSummary> summarizeZipArchive(const QString& archive);

//! False for a member name that would land outside the extraction folder:
//! absolute paths, drive letters, backslashes, ".." components.
SCORE_LIB_BASE_EXPORT
bool isSafeZipMemberName(const QString& name);

//! Inflate a single member of the archive. Empty (and `error` set) on failure,
//! including members larger than `maxSize` bytes (a score file is small).
SCORE_LIB_BASE_EXPORT
QByteArray readZipMember(
    const QString& archive, const QString& member, QString& error,
    quint64 maxSize = 256 * 1024 * 1024);

/**
 * @brief Extract every member of `archive` under `destination`.
 *
 * Members that would escape the destination (absolute paths, "..") are refused.
 * Existing files are overwritten. On failure or cancellation the files written
 * by this call are removed again; pre-existing files are left alone.
 */
SCORE_LIB_BASE_EXPORT
bool extractZipArchive(
    const QString& archive, const QString& destination, QString& error,
    const ZipProgress& progress = {});
}
