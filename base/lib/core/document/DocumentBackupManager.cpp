// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentBackupManager.hpp"

#include "Document.hpp"

#include <core/application/CommandBackupFile.hpp>
#include <core/application/OpenDocumentsFile.hpp>

#include <QFile>
#include <QMap>
#include <QSettings>
#include <QStringList>
#include <QVariant>

score::DocumentBackupManager::DocumentBackupManager(score::Document& doc)
    : QObject{&doc}, m_doc{doc}
{
  m_modelFile.open();

  m_commandFile = new CommandBackupFile{doc.commandStack(), this};
}

score::DocumentBackupManager::~DocumentBackupManager()
{
#if !defined(__EMSCRIPTEN__)
  QSettings s(OpenDocumentsFile::path(), QSettings::IniFormat);
  auto existing_files = s.value("score/docs").toStringList();
  existing_files.removeOne(crashDataFile().fileName());
  s.setValue("score/docs", existing_files);

  QFile(crashDataFile().fileName()).remove();
  QFile(crashCommandFile().fileName()).remove();
#endif
}

void score::DocumentBackupManager::saveModelData(const QByteArray& arr)
{
  m_modelFile.resize(0);
  m_modelFile.reset();
  m_modelFile.write(arr);
  m_modelFile.flush();
}

QTemporaryFile& score::DocumentBackupManager::crashDataFile()
{
  return m_modelFile;
}

score::CommandBackupFile& score::DocumentBackupManager::crashCommandFile()
{
  return *m_commandFile;
}

void score::DocumentBackupManager::updateBackupData()
{
#if !defined(__EMSCRIPTEN__)
  // Save the initial state of the document
  QSettings s{score::OpenDocumentsFile::path(), QSettings::IniFormat};

  auto existing_files = s.value("score/docs").toMap();
  existing_files.insert(
      crashDataFile().fileName(),
      QVariant::fromValue(qMakePair(
          m_doc.metadata().fileName(), crashCommandFile().fileName())));
  s.setValue("score/docs", existing_files);
#endif
}
