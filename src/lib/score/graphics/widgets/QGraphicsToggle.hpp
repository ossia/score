#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QGraphicsItem>
#include <QObject>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsToggle final
    : public QObject
    , public QGraphicsItem
{
  W_OBJECT(QGraphicsToggle)
  Q_INTERFACES(QGraphicsItem)
  QRectF m_rect{defaultToggleSize};

  QString m_textToggled{};
  QString m_textUntoggled{};
  bool m_toggled{};

public:
  QGraphicsToggle(
      const QString& textUntoggled, const QString& textToggled, QGraphicsItem* parent);

  void toggle();
  void setState(bool toggled);
  bool state() const noexcept { return m_toggled; }

  void toggled(bool arg_1) E_SIGNAL(SCORE_LIB_BASE_EXPORT, toggled, arg_1)

  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
      override;
};
}
