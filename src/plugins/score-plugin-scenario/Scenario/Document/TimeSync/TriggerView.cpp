// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TriggerView.hpp"

#include <score/widgets/Pixmap.hpp>
#include <QGraphicsSceneMouseEvent>

#include <wobjectimpl.h>

#include <QPainter>
#include <QBitmap>

W_OBJECT_IMPL(Scenario::TriggerView)
namespace Scenario
{
  static const constexpr int iconSize = 20;

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
  static const QPixmap& triggerSpriteSheet() {
    static const auto p = score::get_pixmap(":/icons/trigger_sprite.png");
    return p;
  }

TriggerView::TriggerView(QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_selected{false}
    , m_hovered{false}
    , m_waiting{false}
    , m_currentFrame{0}
    , m_frameDirection{1}
{
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setAcceptHoverEvents(true);
  this->setAcceptDrops(true);

  m_currentPixmap = triggerPixmap();
  m_image = m_currentPixmap.toImage();
  //connect(m_timer, &QTimer::timeout, this, &TriggerView::nextFrame);
}

void TriggerView::setSelected(bool b) noexcept
{
  m_selected = b;
  updatePixmap();
}

QRectF TriggerView::boundingRect() const
{
  return {0, 0, iconSize,iconSize};
}

void TriggerView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
  painter->drawPixmap(QRect{0,0,iconSize,iconSize}, m_currentPixmap, QRect{m_currentFrame,0,iconSize,iconSize});
}

bool TriggerView::contains(const QPointF& point) const
{
  return m_image.pixelColor(point.x(), point.y()).alpha() > 0 ;
}

void TriggerView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    pressed(event->scenePos());
}

void TriggerView::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  m_hovered = true;
  updatePixmap();
  event->accept();
}

void TriggerView::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  m_hovered = false;
  updatePixmap();
  event->accept();
}

void TriggerView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->scenePos(), *event->mimeData());
}

void TriggerView::nextFrame()
{
  if(!m_waiting)
    return;

  m_currentFrame += m_frameDirection * iconSize;
  if( m_currentFrame >= triggerSpriteSheet().width())
  {
    m_currentFrame = triggerSpriteSheet().width() - iconSize;
    m_frameDirection = -1;
  }
  else if(m_currentFrame < 0)
  {
    m_currentFrame = 0;
    m_frameDirection = 1;
  }
  update();
}

void TriggerView::updatePixmap()
{
  if(m_selected) {
    m_currentPixmap = selectedTriggerPixmap();
  } else if (m_hovered) {
    m_currentPixmap = hoveredTriggerPixmap();
  } else if (m_waiting) {
    m_currentPixmap = triggerSpriteSheet();
  } else {
    m_currentPixmap = triggerPixmap();
  }
}

void TriggerView::onWaitStart()
{
  m_waiting = true;
  updatePixmap();
  update();
}

void TriggerView::onWaitEnd()
{
  m_waiting = false;
  m_currentFrame = 0;
  updatePixmap();
  update();
}
}
