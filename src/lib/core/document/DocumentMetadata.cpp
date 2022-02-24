// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DocumentMetadata.hpp"
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::DocumentMetadata)

namespace score
{

DocumentMetadata::DocumentMetadata(QString file) noexcept
    : m_fileName{file}
{
}

QString DocumentMetadata::fileName() const noexcept
{
  return m_fileName;
}

QString DocumentMetadata::documentName() const noexcept
{
  if(m_fileName.startsWith("Untitle"))
    return m_fileName;

  QFileInfo f{m_fileName};
  if(!f.exists())
    return m_fileName;

  return f.completeBaseName();
}

QString DocumentMetadata::author() const noexcept
{
  return m_author;
}

QDateTime DocumentMetadata::creation() const noexcept
{
  return m_creation;
}

QDateTime DocumentMetadata::lastEdition() const noexcept
{
  return m_lastEdition;
}

QString DocumentMetadata::projectFolder() const noexcept
{
  auto fi = QFileInfo{m_fileName};
  if (fi.exists())
  {
    return fi.absolutePath();
  }

  return {};
}



void DocumentMetadata::setFileName(QString fileName)
{
  if (m_fileName == fileName)
    return;

  m_fileName = fileName;
  fileNameChanged(fileName);
}

void DocumentMetadata::setAuthor(QString author)
{
  if (m_author == author)
    return;

  m_author = author;
  authorChanged(author);
}

void DocumentMetadata::setCreation(QDateTime creation)
{
  if (m_creation == creation)
    return;

  m_creation = creation;
  creationChanged(creation);
}

void DocumentMetadata::setLastEdition(QDateTime lastEdition)
{
  if (m_lastEdition == lastEdition)
    return;

  m_lastEdition = lastEdition;
  lastEditionChanged(lastEdition);
}

QString newProcessedFilePath(const QString& sourceFile, const QDir& destination)
{
  // sourceFile: /tmp/foo-0004.wav

  QFileInfo info{sourceFile};

  if(!destination.exists())
  {
    // Just use the exact name, copied there
    return destination.filePath(info.fileName());
  }
  else
  {
    // wav
    auto ext = info.suffix();

    // foo-0004
    auto name = info.baseName();

    static const QRegularExpression counter{"(.*)-([0-9]+)$"};
    auto m = counter.match(name);
    QString base = name;
    int count = 0;
    if(m.hasMatch())
    {
      base = m.captured(1);
      count = m.captured(2).toInt();
    }

    auto path = [&] (int count) {
      return destination.filePath(
            QStringLiteral("%1-%2.%3")
            .arg(base)
            .arg(count, 4, 10, QChar('0'))
            .arg(ext));
    };
    while(QFile::exists(path(count)))
      ++count;

    return path(count);
  }

}


}
