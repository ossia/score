#pragma once
#include <QGraphicsItem>
#include <Scenario/Document/State/StateView.hpp>
#include <QPainter>
#include <Process/Style/ScenarioStyle.hpp>
#include <QPen>
#include <QBrush>
#include <QGraphicsSceneMouseEvent>

namespace Scenario
{

class StateMenuOverlay final :
    public QGraphicsItem
{
public:
  StateMenuOverlay(StateView* parent):
    QGraphicsItem{parent}
  {
    this->setAcceptHoverEvents(true);

  }

  QRectF boundingRect() const override
  {
    return {-m_radius, -m_radius, 2 * m_radius, 2 * m_radius};
  }

  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
  {
    auto& skin = ScenarioStyle::instance();

    painter->setRenderHint(QPainter::Antialiasing, true);

    const auto& pending_brush = skin.EventPending.getBrush();
    painter->setBrush(pending_brush);
    const auto bright = pending_brush.color();
    QPen p{bright.darker(300)};
    p.setWidth(2);
    painter->setPen(p);

    // TODO instead of a cross, make an arrow that looks like |->
    const auto small_rad = 0.5 * m_radius;
    const QLineF l1{QPointF{0, -small_rad}, QPointF{0, small_rad}};
    const QLineF l2{QPointF{-small_rad, 0}, QPointF{small_rad, 0}};
    painter->drawLine(l1.translated(1,1));
    painter->drawLine(l2.translated(1,1));
    p.setColor(bright);
    painter->setPen(p);
    painter->drawLine(l1);
    painter->drawLine(l2);

    painter->setRenderHint(QPainter::Antialiasing, false);
  }

private:
  void mousePressEvent(QGraphicsSceneMouseEvent* event) override
  {
    auto st = static_cast<StateView*>(parentItem());
    st->startCreateMode();
    event->ignore();
  }

  void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
  {
    prepareGeometryChange();
    m_radius = 6;
    update();
  }
  void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
  {
    prepareGeometryChange();
    m_radius = 4;
    update();
  }

  double m_radius{4};
};

}
