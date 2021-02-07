#pragma once
#include <score/graphics/widgets/Constants.hpp>

#include <QObject>
#include <QGraphicsItem>
#include <verdigris>

#include <score_lib_base_export.h>

namespace score
{
class SCORE_LIB_BASE_EXPORT QGraphicsButton final
    : public QObject,
      public QGraphicsItem
{
  W_OBJECT(QGraphicsButton)
  Q_INTERFACES(QGraphicsItem)
  QRectF m_rect{defaultToggleSize};

  bool m_pressed{};

public:
  QGraphicsButton(QGraphicsItem* parent);

  void bang();

  void pressed(bool b) E_SIGNAL(SCORE_LIB_BASE_EXPORT, pressed, b)

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
};
}

