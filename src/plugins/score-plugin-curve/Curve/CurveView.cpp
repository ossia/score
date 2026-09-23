// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveView.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveStyle.hpp>
#include <Curve/Point/CurvePointModel.hpp>

#include <score/graphics/GraphicsItem.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
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

View::View(QGraphicsItem* parent) noexcept
    : QGraphicsItem{parent}
{
  this->setFlags(ItemIsFocusable);
  this->setZValue(1);
}

View::~View() { }

void View::setModel(const Presenter* p, const Model* m) noexcept
{
  m_presenter = p;
  m_model = m;
}

void View::setDirectDraw(bool d) noexcept
{
  m_directDraw = d;
}

void View::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  if(m_selectArea != QRectF{})
  {
    const QColor sel = score::Skin::instance().Light.color();
    QColor fill = sel;
    fill.setAlpha(40);

    painter->setPen(QPen{sel, 1, Qt::DashLine, Qt::SquareCap, Qt::BevelJoin});
    painter->setBrush(fill);
    painter->drawRect(m_selectArea);
    painter->setBrush(Qt::NoBrush);
  }

  if(m_directDraw)
  {
    auto& style = m_presenter->m_style;
    painter->setPen(
        m_presenter->m_enabled ? style.PenDataset : style.PenDatasetDisabled);

    auto& pts = m_model->points();
    if(pts.size() < 2)
      return;

    if(pts.size() < 1000)
      drawAllPoints(painter);
    else
      drawOptimized(painter);
  }
}

void View::setSelectionArea(const QRectF& rect) noexcept
{
  m_selectArea = rect;
  update();
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if(event->button() == Qt::LeftButton)
  {
    m_pressed = true;
    m_lastScenePos = event->scenePos();
    pressed(event->scenePos());
  }
  event->accept();
}

void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  if(event->button() == Qt::LeftButton)
    doubleClick(event->scenePos());
  event->accept();
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  m_lastScenePos = event->scenePos();
  moved(event->scenePos());
  event->accept();
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  m_pressed = false;
  released(event->scenePos());
  event->accept();
}

bool View::sceneEvent(QEvent* event)
{
  // The scene drops an implicit grab on a move without buttons (the button
  // was let go outside the window) and sends no release: end the drag here.
  // Also sent after every normal release, hence the flag.
  if(event->type() == QEvent::UngrabMouse && m_pressed)
  {
    m_pressed = false;
    released(m_lastScenePos);
  }
  return QGraphicsItem::sceneEvent(event);
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

static constexpr auto curve_view_scale(QPointF first, QSizeF second)
{
  return QPointF{first.x() * second.width(), (1. - first.y()) * second.height()};
}

void View::drawAllPoints(QPainter* painter)
{
  auto& pts = m_model->points();

  auto sz = m_presenter->m_localRect.size();
  auto orig = pts.begin();
  auto next = std::next(orig);
  auto p1 = curve_view_scale((*orig)->pos(), sz);
  auto p2 = curve_view_scale((*next)->pos(), sz);

  painter->drawLine(p1.x(), p1.y(), p2.x(), p2.y());

  p1 = p2;
  for(; next != pts.end(); ++next)
  {
    p2 = curve_view_scale((*next)->pos(), sz);
    painter->drawLine(p1.x(), p1.y(), p2.x(), p2.y());
    p1 = p2;
  }
}

void View::drawOptimized(QPainter* painter)
{
  auto view = getView(*painter);
  if(!view)
    return;

  double x0 = mapFromScene(view->mapToScene(0, 0)).x();
  double xf = mapFromScene(view->mapToScene(view->width(), 0)).x();
  auto& pts = m_model->points();

  auto sz = m_presenter->m_localRect.size();
  int prev_x = INT_MIN;
  auto start_it = std::lower_bound(
      pts.begin(), pts.end(), x0 / sz.width(),
      [](Curve::PointModel* x, double y) { return (*x).pos().x() < y; });

  auto orig = start_it != pts.end() ? start_it : pts.begin();
  auto next = std::next(orig);
  auto p1 = curve_view_scale((*orig)->pos(), sz);
  auto p2 = curve_view_scale((*next)->pos(), sz);

  painter->drawLine(p1.x(), p1.y(), p2.x(), p2.y());

  p1 = p2;
  double accum = 0.;
  int accum_n = 0;
  for(; next != pts.end(); ++next)
  {
    p2 = curve_view_scale((*next)->pos(), sz);
    if(p2.x() < x0)
    {
      p1 = p2;
      continue;
    }
    if(xf < p1.x())
      break;

    int new_x = view->mapFromScene(mapToScene(p1)).x();

    accum += p2.y();
    accum_n++;

    if(new_x != prev_x)
    {
      painter->drawLine(p1.x(), p1.y(), p2.x(), accum / accum_n);
      accum = 0.;
      accum_n = 0;
    }
    prev_x = new_x;
    p1 = p2;
  }
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
  const bool newVisible = m_rect.width() > 5;
  if(newVisible != this->isVisible())
    setVisible(newVisible);
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

  if(!m_tooltip.isEmpty())
  {
    // Compute position
    auto textrect = getTextRect(m_tooltip);

    QPointF pos = QPointF{
        m_tooltipPos.x() * m_defaultW, (1. - m_tooltipPos.y()) * m_rect.height()};
    pos += {10., 10.};
    if(pos.x() + textrect.width() > 0.95 * m_defaultW)
    {
      pos.rx() -= (textrect.width() + 20);
    }
    if(pos.y() + textrect.height() > 0.95 * m_rect.height())
    {
      pos.ry() -= (textrect.height() + 10);
    }

    if(!m_tooltipItem)
    {
      m_tooltipItem = new QGraphicsSimpleTextItem{this};
      m_tooltipItem->setZValue(100);
      const auto& style = Process::Style::instance();
      m_tooltipItem->setFont(score::Skin::instance().Bold10Pt);
      m_tooltipItem->setBrush(style.IntervalBase());
    }

    m_tooltipItem->setText(m_tooltip);
    m_tooltipItem->setPos(pos);
  }
  else
  {
    delete m_tooltipItem;
    m_tooltipItem = nullptr;
  }
}

QPixmap View::pixmap() noexcept
{
  // Retrieve the bounding rect
  QRect rect = boundingRect().toRect();
  if(rect.isNull() || !rect.isValid())
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
  for(QGraphicsItem* child : items)
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
