#include "PatternView.hpp"
#include <Patternist/PatternModel.hpp>

#include <Process/Style/ScenarioStyle.hpp>
#include <score/tools/Bind.hpp>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Patternist::View)
namespace Patternist
{

View::View(
      const Patternist::ProcessModel& model,
      QGraphicsItem* parent)
  : LayerView{parent}
  , m_model{model}
{
  con(model, &Patternist::ProcessModel::patternsChanged,
      this, [=] { update(); });
}

View::~View()
{
}

void View::paint_impl(QPainter* painter) const
{
  if(m_model.currentPattern() > m_model.patterns().size())
    return;

  auto& cur_p = m_model.patterns()[m_model.currentPattern()];
  double lane_height = 40;
  double box_side = 20;
  double box_spacing = 4;
  double x0 = 10;
  double y0 = 30;
  for(int lane = 0; lane < cur_p.lanes.size(); lane++)
  {
    auto& l = cur_p.lanes[lane];
    for(int i = 0; i < cur_p.length; i++)
    {
      const QRectF rect{
        x0 + i * (box_side + box_spacing),
        y0 + lane_height * lane,
        box_side, box_side};

      if(l.pattern[i])
      {
        painter->setPen(Qt::white);
        painter->setBrush(Qt::white);
        painter->drawRect(rect);
      }
      else
      {
        painter->setPen(Qt::white);
        painter->setBrush(Qt::black);
        painter->drawRect(rect);
      }
    }
  }
}


void View::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  auto& cur_p = m_model.patterns()[m_model.currentPattern()];
  double lane_height = 40;
  double box_side = 20;
  double box_spacing = 4;
  double x0 = 10;
  double y0 = 30;
  for(int lane = 0; lane < cur_p.lanes.size(); lane++)
  {
    for(int i = 0; i < cur_p.length; i++)
    {
      const QRectF rect{
        x0 + i * (box_side + box_spacing),
        y0 + lane_height * lane,
        box_side, box_side};

      if(rect.contains(event->pos()))
      {
        toggled(lane, i);
        event->accept();
        return;
      }
    }
  }
  event->accept();
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

}
