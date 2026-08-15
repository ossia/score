#pragma once
#include <Process/FileOperation.hpp>

#include <score/tools/Zip.hpp>

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
 * @brief The files that make up a project, ready to be archived.
 *
 * Built from a consolidation report rather than by listing the project folder:
 * the report knows exactly which files this document uses, so an archive does
 * not sweep up the neighbouring project, last week's backups, or the render
 * someone left in there. It also means trimming shrinks the archive without
 * anything else being aware of it -- the untrimmed originals simply stop being
 * referenced.
 *
 * Everything is placed under one top-level folder named after the document, so
 * that unpacking never scatters files into whatever directory the archive was
 * opened in.
 */
SCORE_LIB_PROCESS_EXPORT
std::vector<score::ZipEntry>
projectArchiveContents(const score::DocumentContext& ctx, const FileReport& report);

//! Total size on disk of an archive's contents, before compression.
SCORE_LIB_PROCESS_EXPORT
qint64 archiveContentsSize(const std::vector<score::ZipEntry>& entries) noexcept;
}
