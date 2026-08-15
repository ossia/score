#include <score/tools/Zip.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <miniz.h>

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
           &zip, e.nameInArchive.toUtf8().constData(),
           e.sourceFile.toUtf8().constData(), nullptr, 0, mz_uint(level)))
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
}
