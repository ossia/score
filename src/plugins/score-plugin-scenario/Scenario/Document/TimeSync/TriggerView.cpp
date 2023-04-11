// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TriggerView.hpp"

#include <Scenario/Document/NetworkMetadata.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <score/model/Skin.hpp>
#include <score/widgets/Pixmap.hpp>

#include <QBitmap>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QGuiApplication>
#include <QPainter>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Scenario::TriggerView)
namespace Scenario
{
static const constexpr int iconSize = 20;

struct TriggerPixmaps
{
  QPixmap trigger = score::get_pixmap(":/icons/scenario_trigger.png");
  QPixmap selectedTrigger = score::get_pixmap(":/icons/scenario_trigger_selected.png");
  QPixmap hoveredTrigger = score::get_pixmap(":/icons/scenario_trigger_hovered.png");
  QPixmap executionSprites = score::get_pixmap(":/icons/trigger_sprite.png");

  QPixmap net_tfi = score::get_pixmap(":/icons/net_20/tfi.png");
  QPixmap net_tfa = score::get_pixmap(":/icons/net_20/tfa.png");

  QPixmap net_tsai = score::get_pixmap(":/icons/net_20/tsai.png");
  QPixmap net_tsaa = score::get_pixmap(":/icons/net_20/tsaa.png");

  QPixmap net_tssi = score::get_pixmap(":/icons/net_20/tssi.png");
  QPixmap net_tssa = score::get_pixmap(":/icons/net_20/tssa.png");

  QImage triggerImage = trigger.toImage();
};

static const TriggerPixmaps& triggerPixmaps()
{
  static const TriggerPixmaps p;
  //p.setDevicePixelRatio(qApp->devicePixelRatio());
  return p;
}
TriggerView::TriggerView(const TimeSyncModel& m, QGraphicsItem* parent)
    : QGraphicsItem{parent}
    , m_model{m}
    , m_currentFrame{0}
    , m_frameDirection{1}
    , m_selected{false}
    , m_hovered{false}
    , m_waiting{false}
{
  auto& skin = score::Skin::instance();
  this->setToolTip(QObject::tr("Trigger\nUsed to introduce temporal interactions."));
  this->setCursor(skin.CursorPointingHand);
  this->setCacheMode(QGraphicsItem::NoCache);
  this->setAcceptHoverEvents(true);
  this->setAcceptDrops(true);

  connect(&m, &TimeSyncModel::networkFlagsChanged, this, [this] { update(); });
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

void TriggerView::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const qreal sz = painter->device()->devicePixelRatioF() * iconSize;
  auto& pixmap = currentPixmap();
  if(&pixmap == &triggerPixmaps().executionSprites)
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
  if(event->button() == Qt::MouseButton::LeftButton)
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
  if(!m_waiting)
    return;

  const auto& pixs = triggerPixmaps();

  const double p = pixs.triggerImage.devicePixelRatio();
  const auto sz = p * iconSize;
  m_currentFrame += m_frameDirection * sz;
  if(m_currentFrame >= pixs.executionSprites.width())
  {
    m_currentFrame = pixs.executionSprites.width() - sz;
    m_frameDirection = -1;
  }
  else if(m_currentFrame < 0)
  {
    m_currentFrame = 0;
    m_frameDirection = 1;
  }
  update();
}

const QPixmap& TriggerView::currentPixmap() const noexcept
{
  const auto& pixs = triggerPixmaps();
  if(m_selected)
  {
    return pixs.selectedTrigger;
  }
  else if(m_hovered)
  {
    return pixs.hoveredTrigger;
  }
  else if(m_waiting)
  {
    return pixs.executionSprites;
  }
  else
  {
    const auto f = Scenario::networkFlags(m_model);
    using enum Process::NetworkFlags;
    if(f & Shared)
    {
      if(f & Compensated)
      {
        if(f & Active)
        {
          [[likely]];
          return pixs.net_tssa;
        }
        else
        {
          return pixs.net_tssi;
        }
      }
      else
      {
        if(f & Active)
        {
          return pixs.net_tsaa;
        }
        else
        {
          return pixs.net_tsai;
        }
      }
    }
    else
    {
      if(f & Active)
      {
        return pixs.net_tfa;
      }
      else
      {
        return pixs.net_tfi;
      }
    }
    [[likely]];

    return pixs.trigger;
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
