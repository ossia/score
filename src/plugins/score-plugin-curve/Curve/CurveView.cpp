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
#include <QStyleOptionGraphicsItem>
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
  // Paint gets the exposed rect: only that is drawn.
  this->setFlags(ItemIsFocusable | ItemUsesExtendedStyleOption);
  this->setZValue(1);
}

View::~View() { }

void View::setModel(const Presenter* p, const Model* m) noexcept
{
  m_presenter = p;
  m_model = m;
  m_pyramidDirty = true;
  if(m)
  {
    connect(m, &Model::changed, this, [this] { m_pyramidDirty = true; });
    connect(m, &Model::curveReset, this, [this] { m_pyramidDirty = true; });
  }
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

    if(m_model->points().size() < 2)
      return;

    drawEnvelope(*painter, option ? option->exposedRect : boundingRect());
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

// Whatever the number of points, O(pixels log N): their envelope per column.
void View::drawEnvelope(QPainter& painter, QRectF exposed)
{
  const auto& pts = m_model->points();
  const auto sz = m_presenter->m_localRect.size();
  if(sz.width() <= 0.)
    return;

  if(m_pyramidDirty)
  {
    m_pyramid.build(pts.size(), [&](std::size_t i) { return pts[i]->pos().y(); });
    m_xs.resize(pts.size());
    for(std::size_t i = 0; i < pts.size(); i++)
      m_xs[i] = pts[i]->pos().x();
    m_pyramidDirty = false;
  }

  const double device = std::abs(painter.deviceTransform().m11());
  const auto cols = pixelColumns(exposed.left(), exposed.right(), device);
  if(cols.count <= 0)
    return;
  const double x0 = cols.first / sz.width();
  const double x1 = (cols.first + cols.count * cols.width) / sz.width();
  auto x = [this](std::size_t i) { return m_xs[i]; };
  auto y = [this](std::size_t i) { return double(m_pyramid.value(i)); };

  // More points than pixels: a waveform, one line per pixel column. Decided
  // for the whole curve, so that a partial repaint draws as the full one.
  if(double(m_xs.size()) > 2. * sz.width() * device)
  {
    static std::vector<QLineF> lines;
    envelopeColumns(
        m_xs.size(), x, y, m_pyramid, 0, x0, cols.width / sz.width(), cols.count,
        lines);
    for(auto& l : lines)
      l = {l.x1() * sz.width(), (1. - l.y1()) * sz.height(), l.x2() * sz.width(),
           (1. - l.y2()) * sz.height()};

    QPen pen{painter.pen().color(), 0.};
    pen.setCosmetic(true);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(pen);
    painter.drawLines(lines.data(), int(lines.size()));
    return;
  }

  static std::vector<QPointF> line;
  envelope(m_xs.size(), x, y, m_pyramid, 0, x0, x1, cols.count, line);
  for(auto& p : line)
    p = {p.x() * sz.width(), (1. - p.y()) * sz.height()};
  painter.drawPolyline(line.data(), int(line.size()));
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
