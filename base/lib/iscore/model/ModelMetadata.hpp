#pragma once
#include <QColor>
#include <QObject>
#include <QString>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/model/ColorReference.hpp>
#include <iscore/tools/Metadata.hpp>
#include <iscore_lib_base_export.h>
#include <qnamespace.h>
namespace iscore
{
/**
 * @brief The ModelMetadata class
 */
class ISCORE_LIB_BASE_EXPORT ModelMetadata : public QObject
{
  ISCORE_SERIALIZE_FRIENDS

  Q_OBJECT
  Q_PROPERTY(QString Name READ getName WRITE setName NOTIFY NameChanged)

  Q_PROPERTY(
      QString Comment READ getComment WRITE setComment NOTIFY CommentChanged)

  Q_PROPERTY(ColorRef Color READ getColor WRITE setColor NOTIFY ColorChanged)

  Q_PROPERTY(QString Label READ getLabel WRITE setLabel NOTIFY LabelChanged)

  Q_PROPERTY(QVariantMap ExtendedMetadata READ getExtendedMetadata WRITE
                 setExtendedMetadata NOTIFY ExtendedMetadataChanged)

public:
  ModelMetadata() = default;
  ModelMetadata(const ModelMetadata& other);

  ModelMetadata& operator=(const ModelMetadata& other);

  const QString& getName() const;
  const QString& getComment() const;
  ColorRef getColor() const;
  const QString& getLabel() const;
  const QVariantMap& getExtendedMetadata() const;

  template <typename T>
  void setInstanceName(const T& t)
  {
    setName(QString("%1.%2")
                .arg(Metadata<PrettyName_k, T>::get())
                .arg(t.id().val()));
  }

  void setName(const QString&);
  void setComment(const QString&);
  void setColor(ColorRef);
  void setLabel(const QString&);
  void setExtendedMetadata(const QVariantMap&);

signals:
  void NameChanged(const QString& arg);
  void CommentChanged(const QString& arg);
  void ColorChanged(ColorRef arg);
  void LabelChanged(const QString& arg);
  void ExtendedMetadataChanged(const QVariantMap& arg);
  void metadataChanged();

private:
  QString m_scriptingName;
  QString m_comment;
  ColorRef m_color;
  QString m_label;
  QVariantMap m_extendedMetadata;
};

ISCORE_PARAMETER_TYPE(ModelMetadata, Name)
ISCORE_PARAMETER_TYPE(ModelMetadata, Comment)
ISCORE_PARAMETER_TYPE(ModelMetadata, Color)
ISCORE_PARAMETER_TYPE(ModelMetadata, Label)
ISCORE_PARAMETER_TYPE(ModelMetadata, ExtendedMetadata)
}
Q_DECLARE_METATYPE(iscore::ModelMetadata)
