#pragma once
#include <score/model/ColorReference.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/tools/Metadata.hpp>

#include <QColor>
#include <QObject>
#include <QString>
#include <qnamespace.h>

#include <score_lib_base_export.h>
#include <wobjectdefs.h>
namespace score
{
/**
 * @brief The ModelMetadata class
 */
class SCORE_LIB_BASE_EXPORT ModelMetadata : public QObject
{
  SCORE_SERIALIZE_FRIENDS

  W_OBJECT(ModelMetadata)

public:
  ModelMetadata();

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

  void NameChanged(const QString& arg)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, NameChanged, arg);
  void CommentChanged(const QString& arg)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, CommentChanged, arg);
  void ColorChanged(score::ColorRef arg)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, ColorChanged, arg);
  void LabelChanged(const QString& arg)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, LabelChanged, arg);
  void ExtendedMetadataChanged(const QVariantMap& arg)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, ExtendedMetadataChanged, arg);
  void metadataChanged() E_SIGNAL(SCORE_LIB_BASE_EXPORT, metadataChanged);

  PROPERTY(
      QString, name READ getName WRITE setName NOTIFY NameChanged, W_Final)
  PROPERTY(
      QString, comment READ getComment WRITE setComment NOTIFY CommentChanged,
      W_Final)
  PROPERTY(
      ColorRef, color READ getColor WRITE setColor NOTIFY ColorChanged,
      W_Final)
  PROPERTY(
      QString, label READ getLabel WRITE setLabel NOTIFY LabelChanged, W_Final)
  PROPERTY(
      QVariantMap,
      extendedMetadata READ getExtendedMetadata WRITE setExtendedMetadata
          NOTIFY ExtendedMetadataChanged,
      W_Final)

private:
  QString m_scriptingName;
  QString m_comment;
  ColorRef m_color;
  QString m_label;
  QVariantMap m_extendedMetadata;
};
}
Q_DECLARE_METATYPE(score::ModelMetadata*)
W_REGISTER_ARGTYPE(score::ModelMetadata*)
