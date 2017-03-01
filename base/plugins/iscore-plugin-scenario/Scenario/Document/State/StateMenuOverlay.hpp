#pragma once
#include <QQuickPaintedItem>
#include <Scenario/Document/State/StateView.hpp>
#include <QPainter>
#include <Process/Style/ScenarioStyle.hpp>
#include <QPen>
#include <QBrush>
#include <QGraphicsSceneMouseEvent>

namespace Scenario
{

class StateMenuOverlay final :
    public QQuickPaintedItem
{
  static const constexpr auto radius = 4;
public:
  StateMenuOverlay(StateView* parent):
    QQuickPaintedItem{parent}
  {
    setWidth(2 * radius);
    setHeight(2 * radius);
  }

  void paint(QPainter* painter) override
  {
    auto& skin = ScenarioStyle::instance();

    painter->setRenderHint(QPainter::Antialiasing, true);

    const auto& pending_brush = skin.EventPending.getColor();
    painter->setBrush(pending_brush);
    const auto bright = pending_brush.color();
    QPen p{bright.darker(300)};
    p.setWidth(2);
    painter->setPen(p);

    const constexpr auto small_rad = 0.5 * radius;
    const QLineF l1{QPointF{0, -small_rad}, QPointF{0, small_rad}};
    const QLineF l2{QPointF{-small_rad, 0}, QPointF{small_rad, 0}};
    painter->drawLine(l1.translated(1,1));
    painter->drawLine(l2.translated(1,1));
    p.setColor(bright);
    painter->setPen(p);
    painter->drawLine(l1);
    painter->drawLine(l2);
  }

protected:
  void mousePressEvent(QMouseEvent* event) override
  {
    auto st = static_cast<StateView*>(parentItem());
    emit st->startCreateMode();
    event->ignore();
  }
};

}
