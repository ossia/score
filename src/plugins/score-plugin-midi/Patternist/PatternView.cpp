#include "PatternView.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Patternist::View)
namespace Patternist
{

View::View(QGraphicsItem* parent)
  : LayerView{parent}
{

}

View::~View()
{
}

void View::paint_impl(QPainter* painter) const
{
}

void View::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
}

void View::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
}

void View::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
}

void View::dropEvent(QGraphicsSceneDragDropEvent* event)
{
}

}

