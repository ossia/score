#pragma once
#include <score/document/DocumentContext.hpp>

#include <QString>

namespace score
{

//! Will also look where the save file is located.
//! TODO in the future also look in a "common" library folder.
SCORE_LIB_BASE_EXPORT
QString
locateFilePath(const QString& filename, const score::DocumentContext& ctx) noexcept;

//! Will try to convert an absolute path
//! in a relative path from the document's point of view
SCORE_LIB_BASE_EXPORT
QString
relativizeFilePath(const QString& filename, const score::DocumentContext& ctx) noexcept;

SCORE_LIB_BASE_EXPORT
QString addUniqueSuffix(const QString& fileName);

//! Where a file / folder picker should open for a control of this document
//! whose current value is @p current: see the PathRoots overload.
SCORE_LIB_BASE_EXPORT
QString
pickerStartFolder(const QString& current, const score::DocumentContext& ctx) noexcept;

//! Same, outside of any document (application settings): the library, the
//! user's documents folder, or the working directory.
SCORE_LIB_BASE_EXPORT
QString pickerStartFolder(const QString& current) noexcept;

struct FilePath
{
  QString absolute;
  QString relative; // Relative to the document root or same than absolute otherwise
  QString filename;
  QString basename;
};

}
