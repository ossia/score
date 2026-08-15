#pragma once
#include <QString>

#include <score_lib_base_export.h>

#include <functional>
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
}
