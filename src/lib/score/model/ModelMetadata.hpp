#pragma once
#include <score/model/ColorReference.hpp>
#include <score/serialization/VisitorInterface.hpp>
#include <score/tools/Metadata.hpp>

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <qnamespace.h>

#include <score_lib_base_export.h>

#include <verdigris>
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

  const QString& getName() const noexcept;
  const QString& getComment() const noexcept;
  ColorRef getColor() const noexcept;
  const QString& getLabel() const noexcept;
  const QVariantMap& getExtendedMetadata() const noexcept;

  template <typename T>
  void setInstanceName(const T& t) noexcept
  {
    setName(QString("%1.%2").arg(Metadata<PrettyName_k, T>::get()).arg(t.id().val()));
    m_touchedName = false;
  }

  bool touchedName() const noexcept;
  void setName(const QString&) noexcept;
  void setComment(const QString&) noexcept;
  void setColor(ColorRef) noexcept;
  void setLabel(const QString&) noexcept;
  void setExtendedMetadata(const QVariantMap&) noexcept;

  void NameChanged(const QString& arg) E_SIGNAL(SCORE_LIB_BASE_EXPORT, NameChanged, arg)
  void CommentChanged(const QString& arg) E_SIGNAL(SCORE_LIB_BASE_EXPORT, CommentChanged, arg)
  void ColorChanged(score::ColorRef arg) E_SIGNAL(SCORE_LIB_BASE_EXPORT, ColorChanged, arg)
  void LabelChanged(const QString& arg) E_SIGNAL(SCORE_LIB_BASE_EXPORT, LabelChanged, arg)
  void ExtendedMetadataChanged(const QVariantMap& arg)
      E_SIGNAL(SCORE_LIB_BASE_EXPORT, ExtendedMetadataChanged, arg)
  void metadataChanged() E_SIGNAL(SCORE_LIB_BASE_EXPORT, metadataChanged)

  PROPERTY(QString, name READ getName WRITE setName NOTIFY NameChanged)
  PROPERTY(QString, comment READ getComment WRITE setComment NOTIFY CommentChanged)
  PROPERTY(ColorRef, color READ getColor WRITE setColor NOTIFY ColorChanged)
  PROPERTY(QString, label READ getLabel WRITE setLabel NOTIFY LabelChanged)
  PROPERTY(
      QVariantMap,
      extendedMetadata READ getExtendedMetadata WRITE setExtendedMetadata NOTIFY
          ExtendedMetadataChanged)

private:
  QString m_scriptingName;
  QString m_comment;
  ColorRef m_color;
  QString m_label;
  QVariantMap m_extendedMetadata;
  bool m_touchedName{};
};
}
Q_DECLARE_METATYPE(score::ModelMetadata*)
W_REGISTER_ARGTYPE(score::ModelMetadata*)
