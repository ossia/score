#include "TimeBar.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <QGuiApplication>
#include <QScreen>
namespace Scenario
{

TimeBar::TimeBar(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  setVisible(false);
  setFlag(QGraphicsItem::ItemIgnoresTransformations);
  setZValue(100000);
}

QRectF TimeBar::boundingRect() const
{
  static const qreal height = 10. * qApp->screens().front()->availableSize().height();
  return {0, 0, 1, height};
}

void TimeBar::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  static const QPen pen(QBrush(Qt::gray), 0);

  painter->setPen(pen);
  painter->drawLine(boundingRect().topLeft(), boundingRect().bottomLeft());
}
}
