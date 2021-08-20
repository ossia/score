// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentBackups.hpp"

#include <score/tools/QMapHelper.hpp>
#include <score/widgets/MessageBox.hpp>

#include <core/application/OpenDocumentsFile.hpp>

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QIcon>
#include <QMap>
#include <QMessageBox>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>

bool score::DocumentBackups::canRestoreDocuments()
{
  // Try to reload if there was a crash
  if (OpenDocumentsFile::exists())
  {
    if (score::question(
            qApp->activeWindow(),
            QObject::tr("Reload?"),
            QObject::tr("It seems that score previously crashed. Do you "
                        "wish to reload your work?"))
        == QMessageBox::Yes)
    {
      return true;
    }
    else
    {
      DocumentBackups::clear();
      return false;
    }
  }

  return false;
}

static void loadRestorableDocumentData(
    const QString& data_filename,
    const QString& save_filename,
    const QString& command_filename,
    std::vector<score::RestorableDocument>& arr)
{
  QFile data_file{data_filename};
  QFile command_file{command_filename};
  if (data_file.exists() && command_file.exists())
  {
    data_file.open(QFile::ReadOnly);
    command_file.open(QFile::ReadOnly);

    auto it = ossia::find_if(arr, [&] (score::RestorableDocument& elt) { return elt.filePath == save_filename; });
    if(it == arr.end())
    {
      arr.push_back(
          {save_filename, data_filename, command_filename, data_file.readAll(), command_file.readAll()});
    }
    else
    {
      // Compare dates
      auto old_time = QFile{it->commandsPath}.fileTime(QFileDevice::FileModificationTime);
      auto new_time = command_file.fileTime(QFileDevice::FileModificationTime);

      if(old_time < new_time)
      {
        it->docPath = data_filename;
        it->commandsPath = command_filename;
        it->doc = data_file.readAll();
        it->commandsPath = command_file.readAll();
      }
    }
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

    auto res = existing_files[file1].value<QPair<QString, QString>>();
    loadRestorableDocumentData(file1, res.first, res.second, arr);
  }
#endif
  return arr;
}

SCORE_LIB_BASE_EXPORT void score::DocumentBackups::clear()
{
#if !defined(__EMSCRIPTEN__)
  if (OpenDocumentsFile::exists())
  {
    // Remove all the tmp files
    {
      QSettings s{score::OpenDocumentsFile::path(), QSettings::IniFormat};

      const auto existing_files = s.value("score/docs").toMap();

      for (auto it = existing_files.cbegin(); it != existing_files.cend(); ++it)
      {
        QFile{it.key()}.remove();
        auto files = it.value().value<QPair<QString, QString>>();
        QFile{files.second}.remove();
      }
    }

    // Remove the file containing the map
    QFile{OpenDocumentsFile::path()}.remove();
  }
#endif
}
