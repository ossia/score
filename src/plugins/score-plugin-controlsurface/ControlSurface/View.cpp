#include "View.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <State/MessageListSerialization.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(ControlSurface::View)
namespace ControlSurface
{

View::View(QGraphicsItem* parent)
  : LayerView{parent}
{
  setZValue(1);
  this->setFlags(ItemClipsToShape | ItemClipsChildrenToShape | ItemIsSelectable | ItemIsFocusable);
}

View::~View() { }

void View::paint_impl(QPainter* painter) const { }

void View::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  pressed(ev->pos());
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();

}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  released(ev->pos());
}

void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  askContextMenu(ev->screenPos(), ev->scenePos());
  ev->accept();
}

void View::dragEnterEvent(QGraphicsSceneDragDropEvent* event) { }

void View::dragLeaveEvent(QGraphicsSceneDragDropEvent* event) { }

void View::dragMoveEvent(QGraphicsSceneDragDropEvent* event) { }

void View::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  Mime<State::MessageList>::Deserializer des(*event->mimeData());
  auto list = des.deserialize();
  if (!list.isEmpty())
    addressesDropped(list);
}

}
