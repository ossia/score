#pragma once
#include <QString>

#include <score_lib_base_export.h>

#include <vector>

namespace score
{
/**
 * @brief A .score file that can be opened as a new, untitled document.
 *
 * Templates and examples are .score files, or project archives (a .zip holding
 * a .score and its media, as written by Project > Archive), found in well-known
 * folders:
 *
 * - Templates: `:/templates`, `<library>/Templates`, `<library>/packages/<pkg>/Templates`
 * - Examples: `<library>/Examples`, `<library>/packages/<pkg>/Examples` (recursive),
 *   and the whole tree of a package that is itself an examples package: its folder
 *   is named `examples` or its package.json has `"kind": "examples"`.
 *
 * where `<library>` is the user library root (Library/RootPath setting).
 * Two files have a special role and are not listed:
 *
 * - `<library>/default.score` is used as the template of every new document.
 * - `first-run.score` in any template folder is the guided score proposed
 *   to first-time users by the start screen.
 */
struct SCORE_LIB_BASE_EXPORT DocumentTemplate
{
  QString name;   //!< File name without extension
  QString path;   //!< Absolute path or resource path
  QString source; //!< "bundled", "library" or the package name
  QString
      category; //!< Folder path relative to the search root, e.g. "reference / processes"
};

//! Root of the user library, from the settings.
SCORE_LIB_BASE_EXPORT
QString libraryRootPath();

//! The template applied to new documents, or an empty string.
SCORE_LIB_BASE_EXPORT
QString defaultDocumentTemplate();

//! The guided score for first-time users, or an empty string.
SCORE_LIB_BASE_EXPORT
QString firstRunDocumentTemplate();

SCORE_LIB_BASE_EXPORT
std::vector<DocumentTemplate> availableDocumentTemplates();

SCORE_LIB_BASE_EXPORT
std::vector<DocumentTemplate> availableExampleDocuments();
}
