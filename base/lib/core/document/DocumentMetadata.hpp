#pragma once
#include <wobjectdefs.h>
#include <QDateTime>
#include <QObject>
#include <QString>

#include <score_lib_base_export.h>
namespace score
{
/**
 * @brief Document-wide metadata
 */
struct SCORE_LIB_BASE_EXPORT DocumentMetadata : public QObject
{
  W_OBJECT(DocumentMetadata)
  Q_PROPERTY(QString fileName READ fileName WRITE setFileName NOTIFY fileNameChanged)
  Q_PROPERTY(QString author READ author WRITE setAuthor NOTIFY authorChanged)
  Q_PROPERTY(QDateTime creation READ creation WRITE setCreation NOTIFY creationChanged)
  Q_PROPERTY(QDateTime lastEdition READ lastEdition WRITE setLastEdition NOTIFY lastEditionChanged)

  QString m_fileName{QObject::tr("Untitled")};
  QString m_author;
  QDateTime m_creation;
  QDateTime m_lastEdition;

public:
  using QObject::QObject;
  DocumentMetadata(QString file);
  QString fileName() const;
  QString author() const;
  QDateTime creation() const;
  QDateTime lastEdition() const;

  void setFileName(QString fileName);
  void setAuthor(QString author);
  void setCreation(QDateTime creation);
  void setLastEdition(QDateTime lastEdition);

  void fileNameChanged(QString fileName)
  W_SIGNAL(fileNameChanged, fileName)
  void authorChanged(QString author)
  W_SIGNAL(authorChanged, author)
  void creationChanged(QDateTime creation)
  W_SIGNAL(creationChanged, creation)
  void lastEditionChanged(QDateTime lastEdition)
  W_SIGNAL(lastEditionChanged, lastEdition)
};
}
