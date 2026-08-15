#include "QGraphicsTimeChooser.hpp"

#include <score/graphics/DefaultGraphicsKnobImpl.hpp>
#include <score/model/Skin.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsTimeChooser)
namespace score
{
namespace
{
struct Division
{
  float fraction; // of a whole note
  const char* label;
};

// Sorted by increasing duration so turning the knob up always means longer.
// The twelve legacy ratios are all present so existing documents round-trip
// exactly.
constexpr Division divisions[] = {
    {1.f / 64.f, "1/64"}, {1.f / 48.f, "1/32T"}, {1.f / 32.f, "1/32"},
    {1.f / 24.f, "1/16T"}, {3.f / 64.f, "1/32."}, {1.f / 16.f, "1/16"},
    {1.f / 12.f, "1/8T"}, {3.f / 32.f, "1/16."}, {1.f / 8.f, "1/8"},
    {1.f / 6.f, "1/4T"}, {3.f / 16.f, "1/8."}, {1.f / 4.f, "1/4"},
    {1.f / 3.f, "1/2T"}, {3.f / 8.f, "1/4."}, {1.f / 2.f, "1/2"},
    {2.f / 3.f, "1/1T"}, {3.f / 4.f, "1/2."}, {1.f, "1/1"},
    {3.f / 2.f, "1/1."}, {2.f, "2/1"}, {3.f, "2/1."}, {4.f, "4/1"},
};
constexpr int division_count = std::ssize(divisions);
constexpr int default_sync_index = 8; // 1/8th

int nearestDivision(float frac) noexcept
{
  int best = 0;
  float bestDist = std::abs(divisions[0].fraction - frac);
  for(int i = 1; i < division_count; i++)
  {
    const float d = std::abs(divisions[i].fraction - frac);
    if(d < bestDist)
    {
      bestDist = d;
      best = i;
    }
  }
  return best;
}
}

QGraphicsTimeChooser::QGraphicsTimeChooser(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton);
  this->setAcceptHoverEvents(true);
  m_other01 = default_sync_index / double(division_count - 1);
}

QGraphicsTimeChooser::~QGraphicsTimeChooser()
{
  if(m_grab)
    sliderReleased();
}

void QGraphicsTimeChooser::setRange(double min, double max, double init)
{
  this->min = min;
  this->max = max;
  this->init = init;
  update();
}

void QGraphicsTimeChooser::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
  update();
}

int QGraphicsTimeChooser::syncIndex() const noexcept
{
  return ossia::clamp(
      int(std::lround(m_value * (division_count - 1))), 0, division_count - 1);
}

void QGraphicsTimeChooser::setValue(ossia::vec2f v)
{
  m_sync = v[1] != 0.f;
  if(m_sync)
    m_value = nearestDivision(v[0]) / double(division_count - 1);
  else
    m_value = ossia::clamp(double(v[0]), 0., 1.);
  update();
}

ossia::vec2f QGraphicsTimeChooser::value() const noexcept
{
  if(m_sync)
    return {divisions[syncIndex()].fraction, 1.f};
  else
    return {float(m_value), 0.f};
}

void QGraphicsTimeChooser::setExecutionValue(ossia::vec2f v)
{
  m_hasExec = true;
  if(v[1] != 0.f)
    m_execValue = nearestDivision(v[0]) / double(division_count - 1);
  else
    m_execValue = ossia::clamp(double(v[0]), 0., 1.);
  update();
}

void QGraphicsTimeChooser::resetExecution()
{
  m_hasExec = false;
  update();
}

void QGraphicsTimeChooser::syncChanged(bool sync)
{
  if(sync == m_sync)
    return;

  // Per-mode memory: come back to where that mode was left
  std::swap(m_value, m_other01);
  m_sync = sync;

  sliderMoved();
  sliderReleased();
  update();
}

QRectF QGraphicsTimeChooser::boundingRect() const
{
  return m_rect;
}

QString QGraphicsTimeChooser::freeText() const
{
  const double secs = min + m_value * (max - min);
  if(secs < 1.)
    return QString::number(secs * 1000., 'f', 0) + QStringLiteral(" ms");
  else if(secs < 10.)
    return QString::number(secs, 'f', 2) + QStringLiteral(" s");
  else
    return QString::number(secs, 'f', 1) + QStringLiteral(" s");
}

void QGraphicsTimeChooser::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  DefaultGraphicsKnobImpl::paint(*this, skin, QString{}, painter, widget);

  // Readout chip: looks like a small button so the free / sync toggle is
  // discoverable. Filled with a note glyph when synced, outlined when free;
  // highlighted on hover.
  const QString text
      = m_sync ? QString::fromLatin1(divisions[syncIndex()].label) : freeText();

  painter->setFont(skin.Medium8Pt);
  const double tw = painter->fontMetrics().horizontalAdvance(text);
  const double glyph_w = m_sync ? 7. : 0.;
  // The knob chord's flat bottom sits at y ~= 24.9: start just below it
  const double chip_w = std::min(m_rect.width(), tw + glyph_w + 8.);
  const QRectF chip{
      m_rect.x() + (m_rect.width() - chip_w) / 2., m_rect.y() + 25.5, chip_w, 9.5};

  painter->setRenderHint(QPainter::Antialiasing, true);
  if(m_sync)
  {
    painter->setPen(skin.NoPen);
    painter->setBrush(
        m_hover ? skin.Emphasis2.lighter.brush : skin.Emphasis2.main.brush);
  }
  else
  {
    painter->setPen(m_hover ? skin.Emphasis2.lighter.pen1 : skin.Emphasis2.main.pen1);
    painter->setBrush(Qt::NoBrush);
  }
  painter->drawRoundedRect(chip, 2., 2.);

  if(m_sync)
  {
    // Note glyph, drawn (sharper than a font glyph at this size)
    const double gx = chip.x() + 3.;
    const double gy = chip.center().y() + 3.;
    painter->setPen(skin.NoPen);
    painter->setBrush(skin.Base4.lighter180.brush);
    painter->drawEllipse(QRectF{gx, gy - 2.5, 3.5, 2.75});
    painter->setPen(skin.Base4.lighter180.pen1);
    painter->drawLine(QPointF{gx + 3.5, gy - 1.5}, QPointF{gx + 3.5, gy - 7.});
  }

  painter->setPen(skin.Base4.lighter180.pen1);
  painter->drawText(
      chip.adjusted(glyph_w, 0., 0., 0.), text, QTextOption(Qt::AlignCenter));
  painter->setRenderHint(QPainter::Antialiasing, false);
}

void QGraphicsTimeChooser::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  m_hover = true;
  update();
  QGraphicsItem::hoverEnterEvent(event);
}

void QGraphicsTimeChooser::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  m_hover = false;
  update();
  QGraphicsItem::hoverLeaveEvent(event);
}

void QGraphicsTimeChooser::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  // The readout row below the knob toggles between free and synced
  if(event->button() == Qt::LeftButton
     && event->pos().y() >= defaultKnobSize.height() - 10.)
  {
    syncChanged(!m_sync);
    event->accept();
    return;
  }
  DefaultGraphicsKnobImpl::mousePressEvent(*this, event);
}

void QGraphicsTimeChooser::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseMoveEvent(*this, event);
}

void QGraphicsTimeChooser::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  // Not DefaultGraphicsKnobImpl::mouseReleaseEvent: its right-click spinbox
  // needs a scalar value() which this widget does not have.
  if(m_grab)
  {
    const double v = InfiniteScroller::move(event);
    if(v != m_value)
    {
      m_value = v;
      update();
    }
    InfiniteScroller::stop(*this, event);
  }
  m_grab = false;
  sliderReleased();
  event->accept();
}

void QGraphicsTimeChooser::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_sync)
    m_value = default_sync_index / double(division_count - 1);
  else
    m_value
        = max != min ? ossia::clamp((init - min) / (max - min), 0., 1.) : 0.;

  m_grab = true;
  sliderMoved();
  sliderReleased();
  m_grab = false;

  update();
  event->accept();
}
}
