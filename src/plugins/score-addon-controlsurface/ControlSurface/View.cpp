#include "View.hpp"

#include <Process/Style/ScenarioStyle.hpp>
#include <State/MessageListSerialization.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(ControlSurface::View)
namespace ControlSurface
{

View::View(QGraphicsItem* parent) : LayerView{parent} { }

View::~View() { }

void View::paint_impl(QPainter* painter) const { }

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
