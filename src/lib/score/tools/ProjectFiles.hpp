#pragma once
#include <QHash>
#include <QSet>
#include <QString>

#include <score_lib_base_export.h>

namespace score
{
struct DocumentContext;

//! Category of an external file referenced by a document.
//! Drives the sub-folder a collected file is copied into.
enum class FileKind
{
  Unknown,
  Audio,
  Video,
  Image,
  Midi,
  Model3D,
  Shader,
  Script,
  Data,
  Folder,
  Plugin
};

//! Guess a file's kind from its extension. Never fails: unknown extensions
//! map to FileKind::Unknown.
SCORE_LIB_BASE_EXPORT
FileKind guessFileKind(const QString& path) noexcept;

//! Name of the project sub-folder collected files of that kind go into.
SCORE_LIB_BASE_EXPORT
QString mediaSubfolder(FileKind k) noexcept;

//! The two roots against which a document path is resolved.
//! Passing them explicitly (instead of a DocumentContext) is what lets the
//! path math be exercised without an application, and lets "save as" resolve
//! against the old folder while relativizing against the new one.
struct SCORE_LIB_BASE_EXPORT PathRoots
{
  //! Absolute path of the document *file* (not its folder). May be empty for
  //! a never-saved document, in which case nothing is project-relative.
  QString documentFile;
  //! Absolute path of the user library root. May be empty.
  QString library;

  //! Absolute path of the folder holding the document.
  QString documentFolder() const noexcept;
};

//! Roots of a live document.
SCORE_LIB_BASE_EXPORT
PathRoots pathRoots(const score::DocumentContext& ctx) noexcept;

//! Resolve a stored path (absolute, document-relative, <PROJECT>: or
//! <LIBRARY>:-prefixed) into an absolute one.
SCORE_LIB_BASE_EXPORT
QString locateFilePath(const QString& filename, const PathRoots& roots) noexcept;

//! Turn an absolute path into a <PROJECT>: / <LIBRARY>: one when it lives
//! under one of the roots. Paths that are already prefixed, or that live
//! elsewhere, come back unchanged.
SCORE_LIB_BASE_EXPORT
QString relativizeFilePath(const QString& filename, const PathRoots& roots) noexcept;

//! True when `path` is `folder` itself or lives below it. Both must be
//! absolute. Used as the containment guard by everything that writes or
//! deletes inside a project.
SCORE_LIB_BASE_EXPORT
bool isUnderFolder(const QString& path, const QString& folder) noexcept;

//! True if the path is one of the tokens score understands as a root.
SCORE_LIB_BASE_EXPORT
bool isProjectRelativePath(const QString& path) noexcept;
SCORE_LIB_BASE_EXPORT
bool isLibraryRelativePath(const QString& path) noexcept;

//! Rewrite `name` into something creatable on every filesystem score runs on:
//! no reserved characters, no Windows device names, no trailing dot or space,
//! and short enough to survive a deep destination folder.
//! Any directory part is dropped: the result is a single path component.
SCORE_LIB_BASE_EXPORT
QString sanitizeFileName(const QString& name) noexcept;

//! Byte-exact comparison. Two paths pointing at the same file compare equal
//! without being read.
SCORE_LIB_BASE_EXPORT
bool sameFileContents(const QString& lhs, const QString& rhs) noexcept;

//! How a collected file is materialized in the project folder.
enum class CopyMode
{
  Copy,     //!< An independent copy. The only mode that survives archiving.
  Symlink,  //!< A symbolic link to the original. Cheap, but not portable.
  Hardlink  //!< A hard link. Same filesystem only.
};

struct SCORE_LIB_BASE_EXPORT ConsolidateOptions
{
  CopyMode mode = CopyMode::Copy;

  //! Also collect files that resolve through the user library (<LIBRARY>:).
  //! Off by default: the library is expected to be installed on the target
  //! machine, and collecting it can mean gigabytes of shared content.
  bool collectLibraryFiles = false;

  //! Group collected files per kind (Audio/, Video/, Images/...) rather than
  //! dropping everything next to the document.
  bool useKindSubfolders = true;

  //! Keep the name of the source's parent folder as an extra level, so that
  //! e.g. Kicks/kick.wav and Snares/kick.wav stay distinguishable by name.
  bool keepSourceFolderName = false;
};

/**
 * @brief Decides where each source file goes inside a project folder.
 *
 * Two properties matter and are both enforced here:
 *
 * - a source referenced N times is placed once (the same destination comes
 *   back for every reference), and
 * - two *different* sources never end up claiming the same destination, even
 *   when they share a file name, and even on a case-insensitive filesystem.
 *
 * A destination that already holds a byte-identical file is reused instead of
 * being duplicated, which is what makes running a consolidation twice a no-op.
 */
class SCORE_LIB_BASE_EXPORT FilePlacement
{
public:
  explicit FilePlacement(QString projectFolder, ConsolidateOptions opts = {}) noexcept;

  struct Placement
  {
    //! Absolute destination path.
    QString destination;
    //! The source already lives inside the project folder: nothing to copy.
    bool alreadyInProject{};
    //! The destination already holds this exact content, or this source was
    //! already placed: nothing to copy.
    bool reused{};
  };

  Placement place(const QString& absoluteSource, FileKind kind);

  const QString& projectFolder() const noexcept { return m_root; }
  const ConsolidateOptions& options() const noexcept { return m_opts; }

private:
  QString subfolderFor(const QString& absoluteSource, FileKind kind) const;

  QString m_root;
  QString m_canonicalRoot;
  ConsolidateOptions m_opts;
  QHash<QString, Placement> m_placed;
  QSet<QString> m_claimed;
};

//! Create `destination` (and its parent folders) from `source` according to
//! `mode`. Returns false and fills `error` on failure; never overwrites an
//! existing destination.
SCORE_LIB_BASE_EXPORT
bool materializeFile(
    const QString& source, const QString& destination, CopyMode mode, QString& error);
}
