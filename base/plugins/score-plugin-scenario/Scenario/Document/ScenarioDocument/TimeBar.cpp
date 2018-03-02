#include "TimeBar.hpp"
#include <Process/Style/ScenarioStyle.hpp>
namespace Scenario
{

TimeBar::TimeBar(QGraphicsItem* parent)
  : QGraphicsItem{parent}
{
  setVisible(false);
  setZValue(100000);
}

QRectF TimeBar::boundingRect() const
{
  return { 0, 0, 1, 2000 };
}

void TimeBar::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  static const auto& pen = ScenarioStyle::instance().SelectedDataCablePen;
  painter->setPen(pen);
  painter->drawLine(boundingRect().topLeft(), boundingRect().bottomLeft());
}

}
