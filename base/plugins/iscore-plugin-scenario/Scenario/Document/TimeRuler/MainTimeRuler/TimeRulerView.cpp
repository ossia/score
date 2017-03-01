#include <Process/Style/ScenarioStyle.hpp>
#include <QColor>
#include <iscore/model/Skin.hpp>

#include "TimeRulerView.hpp"
#include <Scenario/Document/TimeRuler/AbstractTimeRulerView.hpp>
#include <iscore/tools/Todo.hpp>
namespace Scenario
{
TimeRulerView::TimeRulerView(QWidget* v) : AbstractTimeRulerView{v}
{
  //this->setCacheMode(QQuickPaintedItem::NoCache);
  m_height = -3 * m_graduationHeight;
  m_textPosition = 1.65 * m_graduationHeight;
  this->setX(10);
}

QRectF TimeRulerView::boundingRect() const
{
  return QRectF{0, -m_height, m_width * 2, m_height};
}
}
