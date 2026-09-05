#include <score/tools/Zip.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <miniz.h>

#include <memory>

namespace score
{

bool writeZipArchive(
    const QString& destination, const std::vector<ZipEntry>& entries, int level,
    QString& error, const ZipProgress& progress)
{
  if(entries.empty())
  {
    error = QObject::tr("Nothing to archive.");
    return false;
  }

  const QFileInfo dst{destination};
  if(!QDir{}.mkpath(dst.absolutePath()))
  {
    error = QObject::tr("Could not create folder %1").arg(dst.absolutePath());
    return false;
  }

  // Build beside the destination and rename at the end: an interrupted archive
  // that looks like a finished one is how backups get trusted and then fail.
  const QString temporary = destination + QStringLiteral(".part");
  QFile::remove(temporary);

  mz_zip_archive zip{};
  if(!mz_zip_writer_init_file(&zip, temporary.toUtf8().constData(), 0))
  {
    error = QObject::tr("Could not create the archive %1").arg(destination);
    return false;
  }

  const auto abort = [&](QString why) {
    mz_zip_writer_end(&zip);
    QFile::remove(temporary);
    error = std::move(why);
    return false;
  };

  const int total = int(entries.size());
  int done = 0;
  for(const auto& e : entries)
  {
    if(!mz_zip_writer_add_file(
           &zip, e.nameInArchive.toUtf8().constData(), e.sourceFile.toUtf8().constData(),
           nullptr, 0, mz_uint(level)))
    {
      return abort(QObject::tr("Could not add %1 to the archive").arg(e.sourceFile));
    }

    if(progress && !progress(++done, total))
      return abort(QObject::tr("Archiving was cancelled."));
  }

  if(!mz_zip_writer_finalize_archive(&zip))
    return abort(QObject::tr("Could not finalize the archive."));

  mz_zip_writer_end(&zip);

  QFile::remove(destination);
  if(!QFile::rename(temporary, destination))
  {
    QFile::remove(temporary);
    error = QObject::tr("Could not write %1").arg(destination);
    return false;
  }

  return true;
}

namespace
{
struct ZipReader
{
  mz_zip_archive zip{};
  bool ok{};
  explicit ZipReader(const QString& path)
  {
    // Only the central directory at the end of the file is read here.
    ok = mz_zip_reader_init_file(&zip, path.toUtf8().constData(), 0);
  }
  ~ZipReader()
  {
    if(ok)
      mz_zip_reader_end(&zip);
  }
  ZipReader(const ZipReader&) = delete;
  ZipReader& operator=(const ZipReader&) = delete;
};

//! False for names that would land outside the extraction folder
bool isSafeMemberName(const QString& name)
{
  if(name.isEmpty() || name.startsWith('/') || name.contains('\\'))
    return false;
  if(name.size() > 1 && name[1] == ':')
    return false;
  for(const auto& part : name.split('/', Qt::SkipEmptyParts))
    if(part == QStringLiteral(".."))
      return false;
  return true;
}

bool isScoreFileName(const QString& name)
{
  return name.endsWith(QStringLiteral(".score"), Qt::CaseInsensitive)
         || name.endsWith(QStringLiteral(".scorejson"), Qt::CaseInsensitive);
}
}

std::optional<ZipArchiveSummary> summarizeZipArchive(const QString& archive)
{
  ZipReader r{archive};
  if(!r.ok)
    return std::nullopt;

  const QString wanted = QFileInfo{archive}.completeBaseName();
  ZipArchiveSummary res;
  int bestDepth = 2; // root (0) or one folder down (1) only
  bool bestNamedLikeArchive = false;

  const mz_uint n = mz_zip_reader_get_num_files(&r.zip);
  for(mz_uint i = 0; i < n; i++)
  {
    mz_zip_archive_file_stat st;
    if(!mz_zip_reader_file_stat(&r.zip, i, &st))
      continue;
    if(st.m_is_directory)
      continue;

    res.files++;
    res.uncompressedSize += st.m_uncomp_size;

    const QString name = QString::fromUtf8(st.m_filename);
    if(!isScoreFileName(name) || !isSafeMemberName(name))
      continue;

    const int depth = name.count('/');
    const bool named
        = QFileInfo{name}.completeBaseName().compare(wanted, Qt::CaseInsensitive) == 0;
    if(depth < bestDepth || (depth == bestDepth && named && !bestNamedLikeArchive))
    {
      res.scoreFile = name;
      bestDepth = depth;
      bestNamedLikeArchive = named;
    }
  }

  if(res.scoreFile.isEmpty())
    return std::nullopt;
  return res;
}

QByteArray readZipMember(const QString& archive, const QString& member, QString& error)
{
  ZipReader r{archive};
  if(!r.ok)
  {
    error = QObject::tr("%1 is not a zip archive.").arg(archive);
    return {};
  }

  size_t size{};
  void* data = mz_zip_reader_extract_file_to_heap(
      &r.zip, member.toUtf8().constData(), &size, 0);
  if(!data)
  {
    error = QObject::tr("Could not read %1 from %2").arg(member, archive);
    return {};
  }

  QByteArray res{static_cast<const char*>(data), qsizetype(size)};
  mz_free(data);
  return res;
}

bool extractZipArchive(
    const QString& archive, const QString& destination, QString& error,
    const ZipProgress& progress)
{
  ZipReader r{archive};
  if(!r.ok)
  {
    error = QObject::tr("%1 is not a zip archive.").arg(archive);
    return false;
  }

  const QDir dst{destination};
  if(!QDir{}.mkpath(dst.absolutePath()))
  {
    error = QObject::tr("Could not create folder %1").arg(destination);
    return false;
  }

  const mz_uint n = mz_zip_reader_get_num_files(&r.zip);
  for(mz_uint i = 0; i < n; i++)
  {
    mz_zip_archive_file_stat st;
    if(!mz_zip_reader_file_stat(&r.zip, i, &st))
    {
      error = QObject::tr("The archive %1 is damaged.").arg(archive);
      return false;
    }

    const QString name = QString::fromUtf8(st.m_filename);
    if(!isSafeMemberName(name))
    {
      error = QObject::tr("The archive contains an unsafe path: %1").arg(name);
      return false;
    }

    const QString target = dst.filePath(name);
    if(st.m_is_directory)
    {
      if(!QDir{}.mkpath(target))
      {
        error = QObject::tr("Could not create folder %1").arg(target);
        return false;
      }
    }
    else
    {
      if(!QDir{}.mkpath(QFileInfo{target}.absolutePath()))
      {
        error = QObject::tr("Could not create folder %1")
                    .arg(QFileInfo{target}.absolutePath());
        return false;
      }
      if(!mz_zip_reader_extract_to_file(&r.zip, i, target.toUtf8().constData(), 0))
      {
        error = QObject::tr("Could not extract %1").arg(name);
        return false;
      }
    }

    if(progress && !progress(int(i + 1), int(n)))
    {
      error = QObject::tr("Extraction was cancelled.");
      return false;
    }
  }
  return true;
}
}
