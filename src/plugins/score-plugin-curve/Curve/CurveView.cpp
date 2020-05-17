// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveView.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <qnamespace.h>

#include <wobjectimpl.h>
W_OBJECT_IMPL(Curve::View)
namespace Curve
{
static QRectF getTextRect(const QString& txt)
{
  static auto& lay{[]() -> QFontMetricsF& {
    const auto& style = score::Skin::instance();
    static QFontMetricsF lay(style.Bold10Pt);
    return lay;
  }()};

  return lay.boundingRect(txt);
}

View::View(QGraphicsItem* parent) noexcept : QGraphicsItem{parent}
{
  this->setFlags(ItemIsFocusable);
  this->setZValue(1);
}

View::~View() { }

void View::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if (m_selectArea != QRectF{})
  {
    painter->setPen(Qt::white);
    painter->drawRect(m_selectArea);
  }
}

void View::setSelectionArea(const QRectF& rect) noexcept
{
  m_selectArea = rect;
  update();
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
    pressed(event->scenePos());
  event->accept();
}

void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  if (event->button() == Qt::LeftButton)
    doubleClick(event->scenePos());
  event->accept();
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  moved(event->scenePos());
  event->accept();
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  released(event->scenePos());
  event->accept();
}

void View::keyPressEvent(QKeyEvent* ev)
{
  keyPressed(ev->key());
  ev->accept();
}

void View::keyReleaseEvent(QKeyEvent* ev)
{
  keyReleased(ev->key());
  ev->accept();
}

void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  contextMenuRequested(ev->screenPos(), ev->scenePos());
}

void View::setDefaultWidth(double w) noexcept
{
  m_defaultW = w;
  update();
}

void View::setRect(const QRectF& theRect) noexcept
{
  prepareGeometryChange();
  m_rect = theRect;
  setVisible(m_rect.width() > 5);
  update();
}

QRectF View::boundingRect() const
{
  return m_rect;
}

void View::setValueTooltip(QPointF pos, const QString& s) noexcept
{
  m_tooltip = s;
  m_tooltipPos = pos;

  static QGraphicsSimpleTextItem* tooltip{};
  if (!m_tooltip.isEmpty())
  {
    // Compute position
    auto textrect = getTextRect(m_tooltip);

    QPointF pos
        = QPointF{m_tooltipPos.x() * m_defaultW, (1. - m_tooltipPos.y()) * m_rect.height()};
    pos += {10., 10.};
    if (pos.x() + textrect.width() > 0.95 * m_defaultW)
    {
      pos.rx() -= (textrect.width() + 20);
    }
    if (pos.y() + textrect.height() > 0.95 * m_rect.height())
    {
      pos.ry() -= (textrect.height() + 10);
    }

    if (!tooltip)
    {
      tooltip = new QGraphicsSimpleTextItem{this};
      tooltip->setZValue(100);
      const auto& style = Process::Style::instance();
      tooltip->setFont(score::Skin::instance().Bold10Pt);
      tooltip->setBrush(style.IntervalBase());
    }

    tooltip->setText(m_tooltip);
    tooltip->setPos(pos);
  }
  else
  {
    if (tooltip)
    {
      delete tooltip;
      tooltip = nullptr;
    }
  }
}

QPixmap View::pixmap() noexcept
{
  // Retrieve the bounding rect
  QRect rect = boundingRect().toRect();
  if (rect.isNull() || !rect.isValid())
  {
    return QPixmap();
  }

  // Create the pixmap
  QPixmap pixmap(rect.size());
  pixmap.fill(Qt::transparent);

  // Render
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.translate(-rect.topLeft());
  paint(&painter, nullptr, nullptr);
  const auto& items = childItems();
  for (QGraphicsItem* child : items)
  {
    painter.save();
    painter.translate(child->mapToParent(pos()));
    child->paint(&painter, nullptr, nullptr);
    painter.restore();
  }

  painter.end();

  return pixmap;
}
}
