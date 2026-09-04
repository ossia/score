#include <score/tools/ProjectFiles.hpp>

#include <core/document/Document.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QSettings>

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace score
{
static const QString project_prefix = QStringLiteral("<PROJECT>:");
static const QString library_prefix = QStringLiteral("<LIBRARY>:");

bool isProjectRelativePath(const QString& path) noexcept
{
  return path.startsWith(project_prefix);
}

bool isLibraryRelativePath(const QString& path) noexcept
{
  return path.startsWith(library_prefix);
}

FileKind guessFileKind(const QString& path) noexcept
{
  const QString ext = QFileInfo{path}.suffix().toLower();
  if(ext.isEmpty())
    return FileKind::Unknown;

  static const QSet<QString> audio{
      "wav",  "aif", "aiff", "aifc", "flac", "ogg", "oga", "opus", "mp3", "m4a",
      "aac",  "wv",  "ape",  "caf",  "w64",  "rf64", "au", "snd",  "mp2", "wma",
      "voc",  "iff", "8svx", "sf2",  "sfz",  "mpc"};
  static const QSet<QString> video{
      "mp4", "mov", "avi", "mkv", "webm", "mpg", "mpeg", "m4v",
      "wmv", "flv", "ogv", "mxf", "dv",   "m2v", "mts",  "vob"};
  static const QSet<QString> image{
      "png", "jpg", "jpeg", "bmp", "tga", "tif", "tiff", "gif", "webp",
      "exr", "hdr", "dds",  "ppm", "pgm", "pbm", "svg",  "qoi", "ico"};
  static const QSet<QString> midi{"mid", "midi", "smf"};
  static const QSet<QString> model3d{
      "obj", "ply", "stl", "gltf", "glb", "fbx", "dae", "3ds", "pcd", "las", "laz"};
  static const QSet<QString> shader{
      "frag", "vert", "glsl", "fs", "vs", "isf", "comp", "shader", "vsa", "csf"};
  static const QSet<QString> script{
      "js", "mjs", "qml", "pd", "py", "lua", "dsp", "jsfx", "sh", "cpp", "hpp", "c", "h"};
  static const QSet<QString> data{
      "json", "xml", "csv", "tsv", "txt", "yaml", "yml", "h5",  "hdf5",
      "npy",  "sofa", "ttf", "otf", "srt", "gdtf", "ies", "mtl"};

  if(audio.contains(ext))
    return FileKind::Audio;
  if(video.contains(ext))
    return FileKind::Video;
  if(image.contains(ext))
    return FileKind::Image;
  if(midi.contains(ext))
    return FileKind::Midi;
  if(model3d.contains(ext))
    return FileKind::Model3D;
  if(shader.contains(ext))
    return FileKind::Shader;
  if(script.contains(ext))
    return FileKind::Script;
  if(data.contains(ext))
    return FileKind::Data;
  return FileKind::Unknown;
}

QString mediaSubfolder(FileKind k) noexcept
{
  switch(k)
  {
    case FileKind::Audio:
      return QStringLiteral("Audio");
    case FileKind::Video:
      return QStringLiteral("Video");
    case FileKind::Image:
      return QStringLiteral("Images");
    case FileKind::Midi:
      return QStringLiteral("Midi");
    case FileKind::Model3D:
      return QStringLiteral("Models");
    case FileKind::Shader:
      return QStringLiteral("Shaders");
    case FileKind::Script:
      return QStringLiteral("Scripts");
    case FileKind::Data:
      return QStringLiteral("Data");
    case FileKind::Folder:
      return QStringLiteral("Folders");
    case FileKind::Plugin:
      return QStringLiteral("Plugins");
    case FileKind::Unknown:
    default:
      return QStringLiteral("Files");
  }
}

QString PathRoots::documentFolder() const noexcept
{
  if(documentFile.isEmpty())
    return {};

  const QFileInfo fi{documentFile};
  if(fi.exists())
  {
    // Beware: canonicalPath() does not return an empty string for a file that
    // does not exist -- it returns ".", which would silently anchor every
    // project-relative path to the current working directory.
    if(const auto p = fi.canonicalPath(); !p.isEmpty() && p != QStringLiteral("."))
      return p;
  }

  // The document has not been written yet, or is being saved somewhere else:
  // use the lexical parent, resolved if that folder already exists so the
  // result still compares equal to a canonical media path.
  const QString parent = fi.absolutePath();
  if(const auto c = QFileInfo{parent}.canonicalFilePath(); !c.isEmpty())
    return c;
  return parent;
}

PathRoots pathRoots(const score::DocumentContext& ctx) noexcept
{
  PathRoots r;
  r.documentFile = ctx.document.metadata().fileName();

  QSettings set;
  if(auto lib = set.value("Library/RootPath").toString(); QDir{lib}.exists())
    r.library = QFileInfo{lib}.canonicalFilePath();
  return r;
}

QString locateFilePath(const QString& filename, const PathRoots& roots) noexcept
{
  if(filename.isEmpty())
    return {};

  QString path = filename;
  if(isProjectRelativePath(filename))
  {
    const auto folder = roots.documentFolder();
    path.replace(project_prefix, folder.isEmpty() ? QString{} : folder + "/");
  }
  else if(isLibraryRelativePath(filename))
  {
    path.replace(library_prefix, roots.library.isEmpty() ? QString{} : roots.library + "/");
  }
  else if(!QFileInfo{filename}.isAbsolute())
  {
    const auto folder = roots.documentFolder();
    if(folder.isEmpty())
      return QFileInfo{filename}.absoluteFilePath();

    path = folder;
    if(!path.endsWith('/'))
      path += '/';
    path += filename;
  }

  // A path that is already absolute needs no anchoring, and must not be sent
  // through absoluteFilePath() on Windows: a rooted path carrying no drive
  // letter comes back with its leading slash doubled -- "/x.fs" becomes
  // "//x.fs" and "/elsewhere/x.wav" becomes "//elsewhere/x.wav". On Windows a
  // leading "//" is a UNC share, so score would go looking for a host named
  // "x.fs" and can block on a network timeout rather than simply failing to
  // find a local file. cleanPath() normalises separators and "." / ".."
  // without inventing an anchor, and leaves a genuine UNC path alone.
  if(QFileInfo{path}.isAbsolute())
    return QDir::cleanPath(path);

  return QFileInfo{path}.absoluteFilePath();
}

bool isUnderFolder(const QString& path, const QString& root) noexcept
{
  if(root.isEmpty() || path.isEmpty())
    return false;

#if defined(_WIN32)
  constexpr auto cs = Qt::CaseInsensitive;
#else
  constexpr auto cs = Qt::CaseSensitive;
#endif

  if(!path.startsWith(root, cs))
    return false;

  // "/home/me/projA" must not match "/home/me/projAB/x.wav".
  if(path.size() == root.size())
    return true;
  return path[root.size()] == QChar('/') || root.endsWith('/');
}

QString relativizeFilePath(const QString& filename, const PathRoots& roots) noexcept
{
  if(filename.isEmpty())
    return filename;
  if(isProjectRelativePath(filename) || isLibraryRelativePath(filename))
    return filename;

  const QFileInfo info{filename};
  if(!info.isAbsolute())
    return filename;

  // Resolve symlinks so that the comparison matches documentFolder(), which is
  // canonical too. A file that does not exist yet keeps its lexical path.
  QString path = info.canonicalFilePath();
  if(path.isEmpty())
    path = filename;

  // -> QString, not auto: with QT_USE_QSTRINGBUILDER `prefix + out` is an
  // expression template holding references to these locals.
  const auto strip = [&](const QString& root, const QString& prefix) -> QString {
    QString out = path.mid(root.size());
    while(out.startsWith('/'))
      out.remove(0, 1);
    return prefix + out;
  };

  if(const auto folder = roots.documentFolder(); isUnderFolder(path, folder))
    return strip(folder, project_prefix);
  if(isUnderFolder(path, roots.library))
    return strip(roots.library, library_prefix);

  return path;
}

QString sanitizeFileName(const QString& name) noexcept
{
  // Drop any directory part, honouring both separators: a document authored on
  // Windows can carry "C:\media\kick.wav" on a Unix machine.
  QString n = name;
  if(const int i = std::max(n.lastIndexOf('/'), n.lastIndexOf('\\')); i >= 0)
    n = n.mid(i + 1);

  QString out;
  out.reserve(n.size());
  for(const QChar c : std::as_const(n))
  {
    const char16_t u = c.unicode();
    switch(u)
    {
      case u'<':
      case u'>':
      case u':':
      case u'"':
      case u'/':
      case u'\\':
      case u'|':
      case u'?':
      case u'*':
        out += QChar('_');
        continue;
      default:
        break;
    }
    out += (u < 0x20 || u == 0x7f) ? QChar('_') : c;
  }

  // Windows rejects a trailing dot or space outright.
  while(!out.isEmpty() && (out.endsWith('.') || out.endsWith(' ')))
    out.chop(1);
  while(out.startsWith(' '))
    out.remove(0, 1);

  if(out.isEmpty())
    return QStringLiteral("file");

  // Reserved DOS device names are unusable with or without an extension.
  static const QSet<QString> reserved{
      "con",  "prn",  "aux",  "nul",  "com1", "com2", "com3", "com4", "com5",
      "com6", "com7", "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5",
      "lpt6", "lpt7", "lpt8", "lpt9"};
  if(reserved.contains(out.section('.', 0, 0).toLower()))
    out.prepend('_');

  // Leave room for the project folder itself under the Windows 260-char limit.
  constexpr int max_len = 96;
  if(out.size() > max_len)
  {
    const int dot = out.lastIndexOf('.');
    QString suffix = (dot > 0 && out.size() - dot <= 12) ? out.mid(dot) : QString{};
    out.truncate(std::max(qsizetype(1), qsizetype(max_len) - suffix.size()));
    out += suffix;
  }

  return out;
}

bool sameFileContents(const QString& lhs, const QString& rhs) noexcept
{
  if(lhs == rhs)
    return true;

  const QFileInfo a{lhs}, b{rhs};
  if(!a.exists() || !b.exists())
    return false;
  if(a.size() != b.size())
    return false;
  if(const auto ca = a.canonicalFilePath();
     !ca.isEmpty() && ca == b.canonicalFilePath())
    return true;

  QFile fa{lhs}, fb{rhs};
  if(!fa.open(QIODevice::ReadOnly) || !fb.open(QIODevice::ReadOnly))
    return false;

  constexpr qint64 chunk = 64 * 1024;
  QByteArray ba(chunk, Qt::Uninitialized), bb(chunk, Qt::Uninitialized);
  for(;;)
  {
    const qint64 ra = fa.read(ba.data(), chunk);
    const qint64 rb = fb.read(bb.data(), chunk);
    if(ra != rb || ra < 0)
      return false;
    if(ra == 0)
      return true;
    if(std::memcmp(ba.constData(), bb.constData(), ra) != 0)
      return false;
  }
}

FilePlacement::FilePlacement(QString projectFolder, ConsolidateOptions opts) noexcept
    : m_root{std::move(projectFolder)}
    , m_opts{opts}
{
  while(m_root.size() > 1 && m_root.endsWith('/'))
    m_root.chop(1);

  m_canonicalRoot = QFileInfo{m_root}.canonicalFilePath();
  if(m_canonicalRoot.isEmpty())
    m_canonicalRoot = m_root; // does not exist yet: keep the lexical path
  else
    // Build destinations from the resolved root so that they compare equal to
    // the canonical paths relativizeFilePath() works with.
    m_root = m_canonicalRoot;
}

QString FilePlacement::subfolderFor(const QString& absoluteSource, FileKind kind) const
{
  QString sub;
  if(m_opts.useKindSubfolders)
    sub = mediaSubfolder(kind);

  if(m_opts.keepSourceFolderName)
  {
    const QString parent = QFileInfo{absoluteSource}.dir().dirName();
    if(!parent.isEmpty() && parent != QStringLiteral("."))
    {
      const QString clean = sanitizeFileName(parent);
      sub = sub.isEmpty() ? clean : sub + '/' + clean;
    }
  }
  return sub;
}

FilePlacement::Placement
FilePlacement::place(const QString& absoluteSource, FileKind kind)
{
  const QFileInfo src{absoluteSource};
  QString key = src.canonicalFilePath();
  if(key.isEmpty())
    key = src.absoluteFilePath();

  if(const auto it = m_placed.constFind(key); it != m_placed.constEnd())
  {
    Placement p = *it;
    p.reused = true;
    return p;
  }

  Placement out;
  if(isUnderFolder(key, m_canonicalRoot))
  {
    // Already collected (or authored) in the project: leave it in place, which
    // is what makes a second consolidation a no-op.
    out.destination = key;
    out.alreadyInProject = true;
    m_placed.insert(key, out);
    m_claimed.insert(key.toLower());
    return out;
  }

  const QString sub = subfolderFor(key, kind);
  const QString dir = sub.isEmpty() ? m_root : m_root + '/' + sub;
  const QString name = sanitizeFileName(src.fileName());

  const int dot = name.lastIndexOf('.');
  const QString stem = dot > 0 ? name.left(dot) : name;
  const QString suffix = dot > 0 ? name.mid(dot) : QString{};

  for(int i = 0;; ++i)
  {
    const QString candidate
        = i == 0 ? QString{dir + '/' + name}
                 : QString{dir + '/' + stem + QStringLiteral(" (%1)").arg(i) + suffix};

    if(m_claimed.contains(candidate.toLower()))
      continue;

    if(QFileInfo::exists(candidate))
    {
      // Re-use a byte-identical file rather than piling up "kick (1).wav".
      if(!sameFileContents(key, candidate))
        continue;
      out.reused = true;
    }

    out.destination = candidate;
    m_claimed.insert(candidate.toLower());
    m_placed.insert(key, out);
    return out;
  }
}

static std::filesystem::path toStdPath(const QString& s)
{
#if defined(_WIN32)
  return std::filesystem::path{reinterpret_cast<const wchar_t*>(s.utf16())};
#else
  return std::filesystem::path{s.toStdString()};
#endif
}

bool materializeFile(
    const QString& source, const QString& destination, CopyMode mode, QString& error)
{
  const QFileInfo dst{destination};
  if(!QDir{}.mkpath(dst.absolutePath()))
  {
    error = QObject::tr("Could not create folder %1").arg(dst.absolutePath());
    return false;
  }

  if(QFileInfo::exists(destination))
  {
    error = QObject::tr("%1 already exists").arg(destination);
    return false;
  }

  std::error_code ec;
  switch(mode)
  {
    case CopyMode::Copy:
      if(!QFile::copy(source, destination))
      {
        error = QObject::tr("Could not copy %1 to %2").arg(source, destination);
        return false;
      }
      return true;

    case CopyMode::Symlink:
      // Not QFile::link: on Windows that writes a .lnk shortcut, which no
      // media decoder will ever open.
      std::filesystem::create_symlink(toStdPath(source), toStdPath(destination), ec);
      break;

    case CopyMode::Hardlink:
      std::filesystem::create_hard_link(toStdPath(source), toStdPath(destination), ec);
      break;
  }

  if(ec)
  {
    error = QObject::tr("Could not link %1 to %2: %3")
                .arg(source, destination, QString::fromStdString(ec.message()));
    return false;
  }
  return true;
}
}

namespace score
{
QString pickerStartFolder(
    const QString& current, const PathRoots& roots, const QString& userDocuments,
    const QString& workingDir) noexcept
{
  const auto existingDir = [](const QString& p) { return !p.isEmpty() && QDir{p}.exists(); };

  // Where the current file is, if it still is there
  if(!current.isEmpty())
  {
    const QFileInfo fi{locateFilePath(current, roots)};
    if(fi.isDir())
      return fi.absoluteFilePath();
    if(fi.exists())
      return fi.absolutePath();
  }

  if(const auto p = roots.documentFolder(); existingDir(p))
    return p;
  if(existingDir(roots.library))
    return roots.library;
  if(existingDir(userDocuments))
    return userDocuments;
  return workingDir;
}
}
