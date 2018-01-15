#pragma once
#include <QGraphicsItem>
#include <QPainterPath>
#include <QRect>
#include <score/model/ColorReference.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentViewConstants.hpp>

namespace Scenario
{
class ConditionView final
        : public QObject
        , public QGraphicsItem
{
    Q_OBJECT
public:
  ConditionView(score::ColorRef color, QGraphicsItem* parent);

  using QGraphicsItem::QGraphicsItem;
  QRectF boundingRect() const override;
  void paint(
      QPainter* painter,
      const QStyleOptionGraphicsItem* option,
      QWidget* widget) override;
  void changeHeight(qreal newH);

  void setColor(score::ColorRef c)
  {
    m_color = c;
    update();
  }

  static constexpr int static_type()
  { return QGraphicsItem::UserType + ItemType::Condition; }
  int type() const override
  { return static_type(); }

Q_SIGNALS:
  void pressed(QPointF);

private:
  void mousePressEvent(QGraphicsSceneMouseEvent*) override;

  score::ColorRef m_color;
  QPainterPath m_Cpath;
  qreal m_height{0};
  qreal m_width{40};
  qreal m_CHeight{27};
};
}
