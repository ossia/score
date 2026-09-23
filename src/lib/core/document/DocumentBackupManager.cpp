// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentBackupManager.hpp"

#include "Document.hpp"

#include <core/application/CommandBackupFile.hpp>
#include <core/application/OpenDocumentsFile.hpp>

#include <QFile>
#include <QSettings>
#include <QStringList>
#include <QVariant>

namespace score
{
void DocumentBackupManager::storeModelData(const QByteArray& data)
{
#if defined(__EMSCRIPTEN__)
  // A web page keeps nothing of its filesystem; local storage is what survives
  // the tab being closed, which is the only moment a backup matters.
  m_key = QStringLiteral("score-backup/doc-%1").arg(quintptr(this), 0, 16);
  QSettings{}.setValue(m_key, data);
#else
  m_modelFile.open();
  m_modelFile.resize(0);
  m_modelFile.reset();
  m_modelFile.write(data);
  m_modelFile.flush();
#endif
}

DocumentBackupManager::DocumentBackupManager(
    const QByteArray& data, score::Document& doc)
    : QObject{&doc}
    , m_doc{doc}
{
  storeModelData(data);

  m_commandFile = new CommandBackupFile{doc.commandStack(), this};
}

DocumentBackupManager::DocumentBackupManager(
    const score::RestorableDocument& prev, Document& doc)
    : QObject{&doc}
    , m_doc{doc}
{
  storeModelData(prev.doc);

  m_commandFile = new CommandBackupFile{doc.commandStack(), prev.commands, this};
}

DocumentBackupManager::~DocumentBackupManager()
{
#if defined(__EMSCRIPTEN__)
  // Closed normally: this document has nothing left to restore.
  QSettings s;
  auto existing = s.value("score-backup/docs").toMap();
  existing.remove(m_key);
  s.setValue("score-backup/docs", existing);
  s.remove(m_key);
  s.remove(crashCommandFile().fileName());
#else
  // If we are getting there, it means that we could close the document
  // normally thus we can just remove the associated files
  QSettings s(OpenDocumentsFile::path(), QSettings::IniFormat);
  QVariantMap existing_files = s.value("score/docs").toMap();
  existing_files.remove(crashDataFile().fileName());
  s.setValue("score/docs", existing_files);
  s.sync();

  QFile(crashDataFile().fileName()).remove();
  QFile(crashCommandFile().fileName()).remove();
  if(existing_files.empty())
    QFile(OpenDocumentsFile::path()).remove();
#endif
}

QTemporaryFile& DocumentBackupManager::crashDataFile()
{
  return m_modelFile;
}

CommandBackupFile& DocumentBackupManager::crashCommandFile()
{
  return *m_commandFile;
}

QString DocumentBackupManager::commandFileName()
{
  return crashCommandFile().fileName();
}

void DocumentBackupManager::updateBackupData()
{
#if defined(__EMSCRIPTEN__)
  QSettings s;
  auto existing = s.value("score-backup/docs").toMap();
  existing[m_key] = QStringList{m_doc.metadata().fileName(), crashCommandFile().fileName()};
  s.setValue("score-backup/docs", existing);
#else
  // Save the initial state of the document
  QSettings s{OpenDocumentsFile::path(), QSettings::IniFormat};

  auto existing_files = s.value("score/docs").toMap();
  existing_files[crashDataFile().fileName()] = QVariant::fromValue(
      qMakePair(m_doc.metadata().fileName(), crashCommandFile().fileName()));
  s.setValue("score/docs", existing_files);
#endif
}

}