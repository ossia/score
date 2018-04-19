// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentBackups.hpp"

#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QMap>
#include <QMessageBox>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>
#include <core/application/OpenDocumentsFile.hpp>
#include <score/tools/QMapHelper.hpp>

bool score::DocumentBackups::canRestoreDocuments()
{
  // Try to reload if there was a crash
  if (OpenDocumentsFile::exists())
  {
    if (QMessageBox::question(
            qApp->activeWindow(), QObject::tr("Reload?"),
            QObject::tr("It seems that score previously crashed. Do you "
                        "wish to reload your work?"))
        == QMessageBox::Yes)
    {
      return true;
    }
    else
    {
      DocumentBackups::clear();
      // TODO clear tmp folder.
      return false;
    }
  }

  return false;
}

template <typename T>
static void loadRestorableDocumentData(
    const QString& date_filename,
    const QPair<QString, QString>& command_filename,
    T& arr)
{
  QFile data_file{date_filename};
  QFile command_file{command_filename.second};
  if (data_file.exists() && command_file.exists())
  {
    data_file.open(QFile::ReadOnly);
    command_file.open(QFile::ReadOnly);

    arr.push_back(
        {command_filename.first, data_file.readAll(), command_file.readAll()});

    data_file.close();
    data_file.remove(); // Note: maybe we don't want to remove them that early?

    command_file.close();
    command_file.remove();
  }
}

std::vector<score::RestorableDocument>
score::DocumentBackups::restorableDocuments()
{
  std::vector<score::RestorableDocument> arr;

#if !defined(__EMSCRIPTEN__)
  QSettings s{score::OpenDocumentsFile::path(), QSettings::IniFormat};

  auto docs = s.value("score/docs");
  const auto existing_files = docs.toMap();

  for (const auto& file1 : QMap_keys(existing_files))
  {
    if (file1.isEmpty())
      continue;

    loadRestorableDocumentData(
        file1, existing_files[file1].value<QPair<QString, QString>>(), arr);
  }

  s.setValue("score/docs", QMap<QString, QVariant>{});
  s.sync();
#endif
  return arr;
}

SCORE_LIB_BASE_EXPORT void score::DocumentBackups::clear()
{
#if !defined(__EMSCRIPTEN__)
  if (OpenDocumentsFile::exists())
  {
    // Remove all the tmp files
    QSettings s{score::OpenDocumentsFile::path(), QSettings::IniFormat};

    const auto existing_files = s.value("score/docs").toMap();

    for (auto it = existing_files.cbegin(); it != existing_files.cend(); ++it)
    {
      QFile{it.key()}.remove();
      auto files = it.value().value<QPair<QString, QString>>();
      QFile{files.second}.remove();
    }

    // Remove the file containing the map
    QFile{OpenDocumentsFile::path()}.remove();
  }
#endif
}
