// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TriggerView.hpp"

#include <score/widgets/Pixmap.hpp>
#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Scenario::TriggerView)
namespace Scenario
{
  static const QPixmap& triggerPixmap() {
    static const auto p = score::get_pixmap(":/icons/scenario_trigger.png");
    return p;
  }
  static const QPixmap& selectedTriggerPixmap() {
    static const auto p = score::get_pixmap(":/icons/scenario_trigger_selected.png");
    return p;
  }
  static const QPixmap& hoveredTriggerPixmap() {
    static const auto p = score::get_pixmap(":/icons/scenario_trigger_hovered.png");
    return p;
  }
TriggerView::TriggerView(QGraphicsItem* parent)
    : QGraphicsPixmapItem{triggerPixmap(), parent}
    , m_selected{false}
    , m_hovered{false}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setAcceptHoverEvents(true);
  this->setAcceptDrops(true);
}

void TriggerView::setSelected(bool b) noexcept
{
  m_selected = b;
  if(m_selected) {
    setPixmap(selectedTriggerPixmap());
  } else if (m_hovered) {
    setPixmap(hoveredTriggerPixmap());
  } else {
    setPixmap(triggerPixmap());
  }
}

void TriggerView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    pressed(event->scenePos());
}

void TriggerView::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  m_hovered = true;
  if(m_selected) {
    setPixmap(selectedTriggerPixmap());
  } else if (m_hovered) {
    setPixmap(hoveredTriggerPixmap());
  } else {
    setPixmap(triggerPixmap());
  }
  event->accept();
}

void TriggerView::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  m_hovered = false;
  if(m_selected) {
    setPixmap(selectedTriggerPixmap());
  } else if (m_hovered) {
    setPixmap(hoveredTriggerPixmap());
  } else {
    setPixmap(triggerPixmap());
  }
  event->accept();
}

void TriggerView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->scenePos(), *event->mimeData());
}
}
