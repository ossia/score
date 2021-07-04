#pragma once
#include <QDateTime>
#include <QObject>
#include <QString>

#include <score_lib_base_export.h>

#include <verdigris>
class QDir;
namespace score
{
/**
 * @brief Document-wide metadata
 */
struct SCORE_LIB_BASE_EXPORT DocumentMetadata : public QObject
{
  W_OBJECT(DocumentMetadata)
  QString m_fileName{QObject::tr("Untitled")};
  QString m_author;
  QDateTime m_creation;
  QDateTime m_lastEdition;

public:
  using QObject::QObject;
  explicit DocumentMetadata(QString file) noexcept;
  QString fileName() const noexcept;
  QString author() const noexcept;
  QDateTime creation() const noexcept;
  QDateTime lastEdition() const noexcept;

  QString projectFolder() const noexcept;

  void setFileName(QString fileName);
  void setAuthor(QString author);
  void setCreation(QDateTime creation);
  void setLastEdition(QDateTime lastEdition);

  void fileNameChanged(QString fileName)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, fileNameChanged, fileName)
  void authorChanged(QString author)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, authorChanged, author)
  void creationChanged(QDateTime creation)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, creationChanged, creation)
  void lastEditionChanged(QDateTime lastEdition)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, lastEditionChanged, lastEdition)

  W_PROPERTY(
      QString,
      fileName READ fileName WRITE setFileName NOTIFY fileNameChanged,
      W_Final)
  W_PROPERTY(
      QString,
      author READ author WRITE setAuthor NOTIFY authorChanged,
      W_Final)
  W_PROPERTY(
      QDateTime,
      creation READ creation WRITE setCreation NOTIFY creationChanged,
      W_Final)
  W_PROPERTY(
      QDateTime,
      lastEdition READ lastEdition WRITE setLastEdition NOTIFY
          lastEditionChanged,
      W_Final)
};

/**
 * @brief Obtains a new file name in the project folder to save a processed file.
 *
 * e.g. if the source file is /tmp/foo.wav,
 * the output may be
 *
 * /home/myself/Documents/My score project/Processed/foo-0003.wav
 */
SCORE_LIB_BASE_EXPORT
QString newProcessedFilePath(const QString& sourceFile, const QDir& destination);
}
