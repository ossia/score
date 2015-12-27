#include "DocumentMetadata.hpp"

namespace iscore
{

QString DocumentMetadata::fileName() const
{
  return m_fileName;
}

QString DocumentMetadata::author() const
{
  return m_author;
}

QDateTime DocumentMetadata::creation() const
{
  return m_creation;
}

QDateTime DocumentMetadata::lastEdition() const
{
  return m_lastEdition;
}

void DocumentMetadata::setFileName(QString fileName)
{
  if (m_fileName == fileName)
    return;

  m_fileName = fileName;
  emit fileNameChanged(fileName);
}

void DocumentMetadata::setAuthor(QString author)
{
  if (m_author == author)
    return;

  m_author = author;
  emit authorChanged(author);
}

void DocumentMetadata::setCreation(QDateTime creation)
{
  if (m_creation == creation)
    return;

  m_creation = creation;
  emit creationChanged(creation);
}

void DocumentMetadata::setLastEdition(QDateTime lastEdition)
{
  if (m_lastEdition == lastEdition)
    return;

  m_lastEdition = lastEdition;
  emit lastEditionChanged(lastEdition);
}

}
