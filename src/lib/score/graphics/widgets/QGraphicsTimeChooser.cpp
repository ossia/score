#include "QGraphicsTimeChooser.hpp"

#include <score/graphics/DefaultGraphicsKnobImpl.hpp>
#include <score/model/Skin.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <string_view>
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

using Feel = QGraphicsTimeChooser::Feel;

constexpr Feel feelOf(int index) noexcept
{
  std::string_view l = divisions[index].label;
  if(l.back() == '.')
    return Feel::Dotted;
  if(l.back() == 'T')
    return Feel::Triplet;
  return Feel::Straight;
}

// Snap a 0..1 knob position onto a division detent of that feel (any with
// Alt or Shift), so that the knob angle, the emitted value and the execution
// feedback all land on the exact same positions.
double snapSyncPosition(double v01, Feel feel, bool fullTable) noexcept
{
  const double target = ossia::clamp(v01, 0., 1.) * (division_count - 1);
  if(fullTable)
    return std::lround(target) / double(division_count - 1);
  int best = -1;
  double bestDist = 0.;
  for(int i = 0; i < division_count; i++)
  {
    if(feelOf(i) != feel)
      continue;
    const double d = std::abs(target - i);
    if(best < 0 || d < bestDist)
    {
      bestDist = d;
      best = i;
    }
  }
  return best / double(division_count - 1);
}

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

//! The division of that feel nearest to a note length.
int nearestDivision(float frac, Feel feel) noexcept
{
  int best = -1;
  float bestDist = 0.f;
  for(int i = 0; i < division_count; i++)
  {
    if(feelOf(i) != feel)
      continue;
    const float d = std::abs(divisions[i].fraction - frac);
    if(best < 0 || d < bestDist)
    {
      bestDist = d;
      best = i;
    }
  }
  return best;
}

//! How long the note is, straight: a dotted note is 3/2 of it, a triplet 2/3.
float straightLength(int index) noexcept
{
  switch(feelOf(index))
  {
    case Feel::Dotted:
      return divisions[index].fraction / 1.5f;
    case Feel::Triplet:
      return divisions[index].fraction * 1.5f;
    default:
      return divisions[index].fraction;
  }
}

float withFeel(float straight, Feel feel) noexcept
{
  switch(feel)
  {
    case Feel::Dotted:
      return straight * 1.5f;
    case Feel::Triplet:
      return straight / 1.5f;
    default:
      return straight;
  }
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

double QGraphicsTimeChooser::position(ossia::vec2f v) const noexcept
{
  // Only a value in the division domain can be put on a detent: anything else
  // is already a 0..1 position.
  if(m_sync && v[1] != 0.f)
    return nearestDivision(v[0]) / double(division_count - 1);
  return ossia::clamp(double(v[0]), 0., 1.);
}

void QGraphicsTimeChooser::setValue(ossia::vec2f v)
{
  m_sync = v[1] != 0.f;
  m_value = position(v);
  if(m_sync)
    m_feel = feelOf(syncIndex());
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
  m_execValue = position(v);
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
  if(m_sync)
    m_feel = feelOf(syncIndex());

  sliderMoved();
  sliderReleased();
  update();
}

void QGraphicsTimeChooser::cycleMode()
{
  if(!m_sync)
  {
    // Back to the synced value last left, straight
    std::swap(m_value, m_other01);
    m_sync = true;
    const int idx = syncIndex();
    m_feel = Feel::Straight;
    m_value = nearestDivision(straightLength(idx), Feel::Straight)
              / double(division_count - 1);
  }
  else if(m_feel != Feel::Triplet)
  {
    // The same note, dotted then triplet
    const float straight = straightLength(syncIndex());
    m_feel = m_feel == Feel::Straight ? Feel::Dotted : Feel::Triplet;
    m_value = nearestDivision(withFeel(straight, m_feel), m_feel)
              / double(division_count - 1);
  }
  else
  {
    std::swap(m_value, m_other01);
    m_sync = false;
  }

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
  const double secs
      = positionToSeconds ? positionToSeconds(m_value) : min + m_value * (max - min);
  // A few milliseconds matter in an envelope: 0.4 ms is not "0 ms". Decided on
  // the rounded value: 0.01f is 9.99999977 ms, and "10.0 ms" is too wide for
  // the knob, which would cut the unit off.
  if(secs * 1000. < 9.95)
    return QString::number(secs * 1000., 'f', 1) + QStringLiteral(" ms");
  else if(secs < 0.9995)
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
  DefaultGraphicsKnobImpl::paint(*this, skin, QString{}, painter, widget, option);

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
  // The readout row below the knob cycles: free, straight, dotted, triplet
  if(event->button() == Qt::LeftButton
     && event->pos().y() >= defaultKnobSize.height() - 10.)
  {
    cycleMode();
    event->accept();
    return;
  }
  DefaultGraphicsKnobImpl::mousePressEvent(*this, event);
}

void QGraphicsTimeChooser::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  // Not DefaultGraphicsKnobImpl::mouseMoveEvent: in sync mode the position
  // snaps onto the division detents before anything is emitted, so the
  // knob angle always matches a division (and thus the execution feedback).
  if((event->buttons() & Qt::LeftButton) && m_grab)
  {
    double v = InfiniteScroller::move(event);
    if(m_sync)
      v = snapSyncPosition(
          v, m_feel, event->modifiers() & (Qt::AltModifier | Qt::ShiftModifier));
    if(v != m_value)
    {
      m_value = v;
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsTimeChooser::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  // Not DefaultGraphicsKnobImpl::mouseReleaseEvent: its right-click spinbox
  // needs a scalar value() which this widget does not have.
  if(m_grab)
  {
    double v = InfiniteScroller::move(event);
    if(m_sync)
      v = snapSyncPosition(
          v, m_feel, event->modifiers() & (Qt::AltModifier | Qt::ShiftModifier));
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

//! QEvent::UngrabMouse: the scene took the implicit grab away and there will be
//! no release to end the drag on. See DefaultGraphicsSliderImpl for what goes
//! wrong if the edit is left open.
bool QGraphicsTimeChooser::sceneEvent(QEvent* event)
{
  if(event->type() == QEvent::UngrabMouse)
  {
    if(m_grab)
    {
      m_grab = false;
      InfiniteScroller::abort(*this);
      sliderReleased();
    }
  }
  return QGraphicsItem::sceneEvent(event);
}

void QGraphicsTimeChooser::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  if(m_sync)
    m_value = default_sync_index / double(division_count - 1);
  else
    m_value = secondsToPosition ? ossia::clamp(secondsToPosition(init), 0., 1.)
              : max != min ? ossia::clamp((init - min) / (max - min), 0., 1.)
                           : 0.;

  m_grab = true;
  sliderMoved();
  sliderReleased();
  m_grab = false;

  update();
  event->accept();
}
}
