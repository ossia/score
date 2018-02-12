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
  return { 0, 0, 1.5, 2000 };
}

void TimeBar::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  static const auto& brush = ScenarioStyle::instance().SelectedDataCablePen.brush();
  painter->fillRect(boundingRect(), brush);
}

}
