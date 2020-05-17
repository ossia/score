// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TriggerView.hpp"

#include <score/widgets/Pixmap.hpp>

#include <QBitmap>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Scenario::TriggerView)
namespace Scenario
{
static const constexpr int iconSize = 20;

static const QPixmap& triggerPixmap()
{
  static auto p = score::get_pixmap(":/icons/scenario_trigger.png");
  p.setDevicePixelRatio(2.);
  return p;
}
static const QImage& triggerImage()
{
  static auto p = triggerPixmap().toImage();
  return p;
}
static const QPixmap& selectedTriggerPixmap()
{
  static auto p = score::get_pixmap(":/icons/scenario_trigger_selected.png");
  p.setDevicePixelRatio(2.);
  return p;
}
static const QPixmap& hoveredTriggerPixmap()
{
  static auto p = score::get_pixmap(":/icons/scenario_trigger_hovered.png");
  p.setDevicePixelRatio(2.);
  return p;
}
static const QPixmap& triggerSpriteSheet()
{
  static auto p = score::get_pixmap(":/icons/trigger_sprite.png");
  p.setDevicePixelRatio(2.);
  return p;
}

TriggerView::TriggerView(QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_currentFrame{0}
    , m_frameDirection{1}
    , m_selected{false}
    , m_hovered{false}
    , m_waiting{false}
{
  this->setCursor(Qt::OpenHandCursor);
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setAcceptHoverEvents(true);
  this->setAcceptDrops(true);
}

void TriggerView::setSelected(bool b) noexcept
{
  m_selected = b;
  update();
}

QRectF TriggerView::boundingRect() const
{
  return {0, 0, iconSize, iconSize};
}

void TriggerView::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const qreal sz = painter->device()->devicePixelRatioF() * iconSize;
  auto& pixmap = currentPixmap();
  if (&pixmap == &triggerSpriteSheet())
  {
    painter->drawPixmap(QPointF{}, pixmap, QRectF{qreal(m_currentFrame), 0, sz, sz});
    nextFrame();
  }
  else
  {
    painter->drawPixmap(QPointF{}, pixmap);
  }
}

bool TriggerView::contains(const QPointF& point) const
{
  return boundingRect().contains(point);
  // const double p = triggerImage().devicePixelRatio();
  // return triggerImage().pixelColor(point.x() * p, point.y() * p).alpha() > 0
  // ;
}

void TriggerView::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::MouseButton::LeftButton)
    pressed(event->scenePos());
}

void TriggerView::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void TriggerView::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void TriggerView::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  m_hovered = true;
  update();
  event->accept();
}

void TriggerView::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  m_hovered = false;
  update();
  event->accept();
}

void TriggerView::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->scenePos(), *event->mimeData());
}

void TriggerView::nextFrame()
{
  if (!m_waiting)
    return;

  const double p = triggerImage().devicePixelRatio();
  const auto sz = p * iconSize;
  m_currentFrame += m_frameDirection * sz;
  if (m_currentFrame >= triggerSpriteSheet().width())
  {
    m_currentFrame = triggerSpriteSheet().width() - sz;
    m_frameDirection = -1;
  }
  else if (m_currentFrame < 0)
  {
    m_currentFrame = 0;
    m_frameDirection = 1;
  }
  update();
}

const QPixmap& TriggerView::currentPixmap() const noexcept
{
  if (m_selected)
  {
    return selectedTriggerPixmap();
  }
  else if (m_hovered)
  {
    return hoveredTriggerPixmap();
  }
  else if (m_waiting)
  {
    return triggerSpriteSheet();
  }
  else
  {
    return triggerPixmap();
  }
}

void TriggerView::onWaitStart()
{
  m_waiting = true;
  update();
}

void TriggerView::onWaitEnd()
{
  m_waiting = false;
  m_currentFrame = 0;
  update();
}
}
