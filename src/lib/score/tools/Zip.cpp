#include <score/tools/Zip.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <miniz.h>

#include <cstring>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

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

//! Name of a member as stored: UTF-8 when flagged so (bit 11), else the local
//! 8-bit encoding of legacy archivers. Empty for a name miniz had to truncate.
QString memberName(const mz_zip_archive_file_stat& st)
{
  const auto len = std::strlen(st.m_filename);
  if(len + 1 >= sizeof(st.m_filename))
    return {};
  const bool utf8 = st.m_bit_flag & (1 << 11);
  return utf8 ? QString::fromUtf8(st.m_filename, len)
              : QString::fromLocal8Bit(st.m_filename, len);
}

bool isScoreFileName(const QString& name)
{
  return name.endsWith(QStringLiteral(".score"), Qt::CaseInsensitive)
         || name.endsWith(QStringLiteral(".scorejson"), Qt::CaseInsensitive);
}
}

bool isSafeZipMemberName(const QString& name)
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

std::optional<ZipArchiveSummary> summarizeZipArchive(const QString& archive)
{
  ZipReader r{archive};
  if(!r.ok)
    return std::nullopt;

  const QString wanted = QFileInfo{archive}.completeBaseName();
  ZipArchiveSummary res;
  // Best candidate so far: (depth, not named like the archive), lower is better
  std::pair<int, bool> best{std::numeric_limits<int>::max(), true};

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

    const QString name = memberName(st);
    if(!isScoreFileName(name) || !isSafeZipMemberName(name))
      continue;

    const int depth = name.count('/');
    if(depth > 1) // root or one folder down only
      continue;
    const bool named
        = QFileInfo{name}.completeBaseName().compare(wanted, Qt::CaseInsensitive) == 0;
    if(const std::pair<int, bool> candidate{depth, !named}; candidate < best)
    {
      res.scoreFile = name;
      best = candidate;
    }
  }

  if(res.scoreFile.isEmpty())
    return std::nullopt;
  return res;
}

QByteArray readZipMember(
    const QString& archive, const QString& member, QString& error, quint64 maxSize)
{
  ZipReader r{archive};
  if(!r.ok)
  {
    error = QObject::tr("%1 is not a zip archive.").arg(archive);
    return {};
  }

  // The declared size comes from the archive: never trust it blindly
  const int index = mz_zip_reader_locate_file(
      &r.zip, member.toUtf8().constData(), nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE);
  mz_zip_archive_file_stat st;
  if(index < 0 || !mz_zip_reader_file_stat(&r.zip, mz_uint(index), &st))
  {
    error = QObject::tr("Could not read %1 from %2").arg(member, archive);
    return {};
  }
  if(st.m_uncomp_size > maxSize)
  {
    error = QObject::tr("%1 in %2 is too large (%3 bytes)")
                .arg(member, archive)
                .arg(st.m_uncomp_size);
    return {};
  }

  size_t size{};
  void* data = mz_zip_reader_extract_to_heap(&r.zip, mz_uint(index), &size, 0);
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

  // Half an extraction is worse than none: whatever this call wrote is
  // removed if it does not complete. Folders are only removed when empty, so
  // that a pre-existing folder the user chose is left as it was.
  std::vector<QString> writtenFiles;
  std::vector<QString> createdDirs;
  const auto mkpath = [&](const QString& dir) {
    if(QDir{dir}.exists())
      return true;
    if(!QDir{}.mkpath(dir))
      return false;
    createdDirs.push_back(dir);
    return true;
  };
  const auto abort = [&](QString why) {
    for(const auto& f : writtenFiles)
      QFile::remove(f);
    for(auto it = createdDirs.rbegin(); it != createdDirs.rend(); ++it)
      QDir{}.rmdir(*it);
    error = std::move(why);
    return false;
  };

  if(!mkpath(dst.absolutePath()))
  {
    error = QObject::tr("Could not create folder %1").arg(destination);
    return false;
  }

  const mz_uint n = mz_zip_reader_get_num_files(&r.zip);
  for(mz_uint i = 0; i < n; i++)
  {
    mz_zip_archive_file_stat st;
    if(!mz_zip_reader_file_stat(&r.zip, i, &st))
      return abort(QObject::tr("The archive %1 is damaged.").arg(archive));

    const QString name = memberName(st);
    if(!isSafeZipMemberName(name))
      return abort(QObject::tr("The archive contains an unsafe path: %1").arg(name));

    const QString target = dst.filePath(name);
    if(st.m_is_directory)
    {
      if(!mkpath(target))
        return abort(QObject::tr("Could not create folder %1").arg(target));
    }
    else
    {
      const QString parent = QFileInfo{target}.absolutePath();
      if(!mkpath(parent))
        return abort(QObject::tr("Could not create folder %1").arg(parent));
      const bool existed = QFile::exists(target);
      if(!mz_zip_reader_extract_to_file(&r.zip, i, target.toUtf8().constData(), 0))
      {
        if(!existed)
          writtenFiles.push_back(target);
        return abort(QObject::tr("Could not extract %1").arg(name));
      }
      if(!existed)
        writtenFiles.push_back(target);
    }

    if(progress && !progress(int(i + 1), int(n)))
      return abort(QObject::tr("Extraction was cancelled."));
  }
  return true;
}
}
