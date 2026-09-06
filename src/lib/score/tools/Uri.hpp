#pragma once
#include <QString>

#include <score_lib_base_export.h>

namespace score
{
struct DocumentContext;

//! How a path stored in a document is expressed.
enum class UriScheme
{
  //! A path on this machine, and meaningful on no other. Documents that hold
  //! these do not survive being moved.
  Absolute,

  //! Relative to the document's folder, from before the tokens below existed.
  //! Equivalent to Project, and still written by nothing.
  Relative,

  //! Under the document's own folder: "<PROJECT>:".
  Project,

  //! Under the user's library: "<LIBRARY>:".
  Library,

  //! Content-addressed media: "<CACHE>:". Not authored by hand -- these name a
  //! file by its hash, so the same media is the same entry on every machine
  //! that has it, and a machine that does not can be told exactly what to
  //! fetch.
  Cache
};

/**
 * @brief A path as a document stores it.
 *
 * score already wrote "<PROJECT>:" and "<LIBRARY>:" into save files; this puts
 * a name on that and makes the two directions -- resolving and relativizing --
 * one thing rather than two functions that had to be kept agreeing.
 */
struct SCORE_LIB_BASE_EXPORT Uri
{
  //! Read what a document stored. Never fails: anything unrecognised is an
  //! absolute or relative path, which is what old documents contain.
  static Uri parse(const QString& stored) noexcept;

  //! Express an absolute path in the most portable scheme that fits.
  static Uri relativize(const QString& absolute, const DocumentContext& ctx) noexcept;

  //! What to store in a document.
  QString toString() const noexcept;

  //! Where to read it from on this machine. Empty if it cannot be placed.
  QString resolve(const DocumentContext& ctx) const noexcept;

  //! Whether opening the document elsewhere can still find this.
  bool isPortable() const noexcept { return scheme != UriScheme::Absolute; }

  UriScheme scheme{UriScheme::Absolute};

  //! The remainder after the scheme, or the whole path when there is none.
  QString path;
};

//! Where "<CACHE>:" resolves to.
SCORE_LIB_BASE_EXPORT QString mediaCacheRoot() noexcept;

//! Whether `path` is `dir` itself or something under it.
//!
//! Distinct from a prefix test, which answers yes for "/a/proj2" under
//! "/a/proj" and would then relativize it to a path meaning a different file.
SCORE_LIB_BASE_EXPORT bool isUnder(const QString& path, const QString& dir) noexcept;
}
