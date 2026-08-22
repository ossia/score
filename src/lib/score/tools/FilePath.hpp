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

//! Same, for callers that run before any document exists -- a --script file is
//! read while the application is still starting up. <LIBRARY>: is a settings
//! lookup and resolves fine; <PROJECT>: and relative paths have no document to
//! resolve against and are returned untouched.
SCORE_LIB_BASE_EXPORT
QString locateFilePath(const QString& filename) noexcept;

//! Will try to convert an absolute path
//! in a relative path from the document's point of view
SCORE_LIB_BASE_EXPORT
QString
relativizeFilePath(const QString& filename, const score::DocumentContext& ctx) noexcept;

SCORE_LIB_BASE_EXPORT
QString addUniqueSuffix(const QString& fileName);

struct FilePath
{
  QString absolute;
  QString relative; // Relative to the document root or same than absolute otherwise
  QString filename;
  QString basename;
};

}
