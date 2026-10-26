#include "QGraphicsGridChooser.hpp"

#include <score/graphics/DefaultGraphicsKnobImpl.hpp>
#include <score/model/Skin.hpp>

#include <ossia/detail/math.hpp>

#include <QActionGroup>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QPainter>

#include <cmath>
#include <wobjectimpl.h>

W_OBJECT_IMPL(score::QGraphicsGridChooser)
namespace score
{
namespace
{
constexpr int periodCount = int(QGraphicsGridChooser::periodDetents.size());

int nearestPeriod(double v) noexcept
{
  int best = 0;
  double bestDist = 1e300;
  for(int i = 0; i < periodCount; i++)
  {
    // Compared as ratios: 1/4 is as far from 1/2 as 4 is from 2
    const double d
        = std::abs(std::log(std::max(v, 1e-9) / QGraphicsGridChooser::periodDetents[i]));
    if(d < bestDist)
    {
      bestDist = d;
      best = i;
    }
  }
  return best;
}

bool near(double a, double b) noexcept
{
  return std::abs(a - b) <= 1e-4 * std::max(std::abs(a), std::abs(b));
}

//! A number as short as it can be written: 3, 1.5, 0.375
QString number(double v)
{
  return QString::number(v, 'g', 4);
}

QString secondsLabel(double secs)
{
  if(!(secs > 0.))
    return QStringLiteral("None");
  // Decided on the rounded value, as the time chooser: "10.0 ms" would be too
  // wide for the knob.
  if(secs * 1000. < 9.95)
    return QString::number(secs * 1000., 'f', 1) + QStringLiteral(" ms");
  else if(secs < 0.9995)
    return QString::number(secs * 1000., 'f', 0) + QStringLiteral(" ms");
  else if(secs < 10.)
    return QString::number(secs, 'f', 2) + QStringLiteral(" s");
  else
    return QString::number(secs, 'f', 1) + QStringLiteral(" s");
}

QString noteLabel(double frac)
{
  if(!(frac > 0.))
    return QStringLiteral("None");
  const int idx = note_values::nearest(float(frac));
  if(near(note_values::table[idx].fraction, frac))
    return QString::fromLatin1(note_values::table[idx].label);
  // Off the table, e.g. from a preset: 1/5, 3/1, or the fraction itself
  const double inv = 1. / frac;
  if(near(inv, std::round(inv)))
    return QStringLiteral("1/%1").arg(std::lround(inv));
  if(near(frac, std::round(frac)))
    return QStringLiteral("%1/1").arg(std::lround(frac));
  return number(frac) + QStringLiteral("/1");
}

QString periodLabel(double m, const QString& name)
{
  if(!(m > 0.))
    return QStringLiteral("None");
  if(near(m, 1.))
  {
    QString n = name;
    if(!n.isEmpty())
      n[0] = n[0].toUpper();
    return n;
  }
  const double inv = 1. / m;
  if(m < 1. && near(inv, std::round(inv)))
    return QStringLiteral("1/%1 %2").arg(std::lround(inv)).arg(name);
  return number(m) + QStringLiteral("x ") + name;
}

int slotOf(QGraphicsGridChooser::Unit u) noexcept
{
  return ossia::clamp(int(u), 0, 3);
}
}

QGraphicsGridChooser::QGraphicsGridChooser(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  this->setAcceptHoverEvents(true);
  m_value = positionOf(m_unit, m_values[slotOf(m_unit)]);
}

QGraphicsGridChooser::~QGraphicsGridChooser()
{
  if(m_grab)
    sliderReleased();
}

void QGraphicsGridChooser::setUnits(uint32_t units)
{
  m_units = units;
  update();
}

void QGraphicsGridChooser::setMaxBars(int bars)
{
  m_maxBars = std::max(1, bars);
  m_value = positionOf(m_unit, m_values[slotOf(m_unit)]);
  update();
}

void QGraphicsGridChooser::setPeriodName(const QString& name)
{
  m_periodName = name;
  update();
}

void QGraphicsGridChooser::setMaxSeconds(double seconds)
{
  max = seconds > 0. ? seconds : 1.;
  if(m_unit == Free)
    m_value = unmap(m_values[0]);
  update();
}

void QGraphicsGridChooser::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
  update();
}

double QGraphicsGridChooser::map(double position) const noexcept
{
  const double p = ossia::clamp(position, 0., 1.);
  return min + p * p * p * (max - min);
}

double QGraphicsGridChooser::unmap(double seconds) const noexcept
{
  if(!(max > min))
    return 0.;
  return std::cbrt(ossia::clamp((seconds - min) / (max - min), 0., 1.));
}

double QGraphicsGridChooser::positionOf(Unit u, double value) const noexcept
{
  switch(u)
  {
    case Free:
      return unmap(value);
    case Note:
      return note_values::position(note_values::nearest(float(value)));
    case Bars:
      if(m_maxBars <= 1)
        return 0.;
      return (ossia::clamp(double(std::lround(value)), 1., double(m_maxBars)) - 1.)
             / double(m_maxBars - 1);
    case Period:
      return nearestPeriod(value) / double(periodCount - 1);
    default:
      return 0.;
  }
}

double QGraphicsGridChooser::valueAt(Unit u, double position) const noexcept
{
  const double p = ossia::clamp(position, 0., 1.);
  switch(u)
  {
    case Free:
      return map(p);
    case Note:
      return note_values::table[note_values::indexAt(p)].fraction;
    case Bars:
      return 1. + std::lround(p * (m_maxBars - 1));
    case Period:
      return periodDetents[std::lround(p * (periodCount - 1))];
    default:
      return 0.;
  }
}

double QGraphicsGridChooser::snap(double position, bool allNoteValues) const noexcept
{
  const double p = ossia::clamp(position, 0., 1.);
  switch(m_unit)
  {
    case Note:
      return note_values::snapPosition(p, m_feel, allNoteValues);
    case Bars:
      return positionOf(Bars, valueAt(Bars, p));
    case Period:
      return std::lround(p * (periodCount - 1)) / double(periodCount - 1);
    case Free:
      return p;
    default:
      return 0.;
  }
}

void QGraphicsGridChooser::setValue(ossia::vec2f v)
{
  const int u = int(std::lround(v[1]));
  m_unit = u < 0 ? Parent : u > 3 ? Free : Unit(u);
  if(m_unit != Parent)
    m_values[slotOf(m_unit)] = v[0];
  m_value = positionOf(m_unit, v[0]);
  if(m_unit == Note)
    m_feel = note_values::feelOf(note_values::indexAt(m_value));
  update();
}

ossia::vec2f QGraphicsGridChooser::value() const noexcept
{
  switch(m_unit)
  {
    case Parent:
      return {0.f, float(Parent)};
    case Free:
      // The knob is the truth here: the right-click box sets only the position
      return {float(map(m_value)), float(Free)};
    default:
      return {float(m_values[slotOf(m_unit)]), float(m_unit)};
  }
}

void QGraphicsGridChooser::setExecutionValue(ossia::vec2f v)
{
  const int u = int(std::lround(v[1]));
  if(u != int(m_unit))
  {
    resetExecution();
    return;
  }
  m_hasExec = true;
  m_execValue = positionOf(m_unit, v[0]);
  update();
}

void QGraphicsGridChooser::resetExecution()
{
  m_hasExec = false;
  update();
}

void QGraphicsGridChooser::edited()
{
  sliderMoved();
  sliderReleased();
  update();
}

void QGraphicsGridChooser::chooseUnit(Unit u)
{
  if(u == m_unit)
    return;
  if(m_unit == Free)
    m_values[0] = map(m_value);
  m_unit = u;
  m_value = u == Parent ? 0. : positionOf(u, m_values[slotOf(u)]);
  if(u == Note)
    m_feel = note_values::feelOf(note_values::indexAt(m_value));
  edited();
}

void QGraphicsGridChooser::chooseFeel(Feel f)
{
  if(m_unit != Note || f == m_feel)
    return;
  const float straight = note_values::straightLength(note_values::indexAt(m_value));
  m_feel = f;
  const int idx = note_values::nearest(note_values::withFeel(straight, f), f);
  m_value = note_values::position(idx);
  m_values[slotOf(Note)] = note_values::table[idx].fraction;
  edited();
}

QString QGraphicsGridChooser::label(ossia::vec2f v, const QString& periodName)
{
  switch(int(std::lround(v[1])))
  {
    case Parent:
      return QStringLiteral("Parent");
    case Note:
      return noteLabel(v[0]);
    case Bars:
      if(!(v[0] > 0.f))
        return QStringLiteral("None");
      return near(v[0], 1.) ? QStringLiteral("1 bar")
                            : number(v[0]) + QStringLiteral(" bars");
    case Period:
      return periodLabel(v[0], periodName);
    default:
      return secondsLabel(v[0]);
  }
}

QString QGraphicsGridChooser::unitName(Unit u, const QString& periodName)
{
  switch(u)
  {
    case Parent:
      return QObject::tr("Parent");
    case Free:
      return QObject::tr("Free time");
    case Note:
      return QObject::tr("Note value");
    case Bars:
      return QObject::tr("Bars");
    case Period: {
      QString n = periodName;
      if(!n.isEmpty())
        n[0] = n[0].toUpper();
      return n;
    }
  }
  return {};
}

QString QGraphicsGridChooser::text() const
{
  return label(value(), m_periodName);
}

QMenu* QGraphicsGridChooser::makeUnitMenu()
{
  auto menu = new QMenu;
  auto units = new QActionGroup{menu};
  for(Unit u : {Free, Note, Bars, Period, Parent})
  {
    if(!(m_units & flag(u)) && u != m_unit)
      continue;
    auto act = menu->addAction(unitName(u, m_periodName));
    act->setCheckable(true);
    act->setChecked(u == m_unit);
    act->setActionGroup(units);
    connect(act, &QAction::triggered, this, [this, u] { chooseUnit(u); });
  }

  if(m_unit == Note)
  {
    menu->addSeparator();
    auto feels = new QActionGroup{menu};
    const std::pair<Feel, QString> names[]
        = {{Feel::Straight, QObject::tr("Straight")},
           {Feel::Dotted, QObject::tr("Dotted")},
           {Feel::Triplet, QObject::tr("Triplet")}};
    for(const auto& [f, name] : names)
    {
      auto act = menu->addAction(name);
      act->setCheckable(true);
      act->setChecked(f == m_feel);
      act->setActionGroup(feels);
      connect(act, &QAction::triggered, this, [this, f] { chooseFeel(f); });
    }
  }
  return menu;
}

void QGraphicsGridChooser::popupUnitMenu(QPoint screenPos)
{
  auto menu = makeUnitMenu();
  menu->setAttribute(Qt::WA_DeleteOnClose);
  menu->popup(screenPos);
}

QRectF QGraphicsGridChooser::boundingRect() const
{
  // The footprint of a knob and of the time chooser: the dial, and 7 below.
  return QRectF{m_rect.x(), m_rect.y(), m_rect.width(), m_rect.height() + 7.};
}

void QGraphicsGridChooser::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  DefaultGraphicsKnobImpl::paint(*this, skin, QString{}, painter, widget, option);

  // Readout chip, as the time chooser's: outlined for a free time, filled for
  // a musical grid, with a note glyph for a note value.
  const QString txt = text();
  const bool musical = m_unit != Free;
  const bool glyph = m_unit == Note;

  painter->setFont(skin.Medium8Pt);
  const double tw = painter->fontMetrics().horizontalAdvance(txt);
  const double glyph_w = glyph ? 7. : 0.;
  const double chip_w = std::min(m_rect.width() - 2., tw + glyph_w + 8.);
  const QRectF chip{
      m_rect.x() + (m_rect.width() - chip_w) / 2., m_rect.y() + 25.5, chip_w, 9.5};

  painter->setRenderHint(QPainter::Antialiasing, true);
  if(musical)
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

  if(glyph)
  {
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
      chip.adjusted(glyph_w, 0., 0., 0.), txt, QTextOption(Qt::AlignCenter));
  painter->setRenderHint(QPainter::Antialiasing, false);
}

void QGraphicsGridChooser::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  m_hover = true;
  update();
  QGraphicsItem::hoverEnterEvent(event);
}

void QGraphicsGridChooser::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  m_hover = false;
  update();
  QGraphicsItem::hoverLeaveEvent(event);
}

bool QGraphicsGridChooser::onReadout(QPointF pos) const noexcept
{
  return pos.y() >= defaultKnobSize.height() - 10.;
}

void QGraphicsGridChooser::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  switch(event->button())
  {
    case Qt::LeftButton:
      // The readout chooses the unit
      if(onReadout(event->pos()))
      {
        popupUnitMenu(event->screenPos());
        event->accept();
        return;
      }
      // The parent's grid has nothing to turn
      if(m_unit == Parent)
      {
        event->accept();
        return;
      }
      break;
    case Qt::RightButton:
      // Typing a time in: seconds only, the other units are picked on the knob
      if(m_unit != Free)
      {
        event->ignore();
        return;
      }
      break;
    default:
      break;
  }
  DefaultGraphicsKnobImpl::mousePressEvent(*this, event);
}

void QGraphicsGridChooser::dragTo(double position, bool allNoteValues)
{
  const double v = snap(position, allNoteValues);
  if(v == m_value)
    return;
  m_value = v;
  if(m_unit != Free && m_unit != Parent)
    m_values[slotOf(m_unit)] = valueAt(m_unit, v);
  sliderMoved();
  update();
}

void QGraphicsGridChooser::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  // The position snaps onto the unit's detents before anything is emitted, so
  // the knob angle always matches a value (and thus the execution feedback).
  if((event->buttons() & Qt::LeftButton) && m_grab)
    dragTo(
        InfiniteScroller::move(event),
        event->modifiers() & (Qt::AltModifier | Qt::ShiftModifier));
  event->accept();
}

void QGraphicsGridChooser::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  // Only a drag has an edit to end here: a click on the readout or a
  // double-click ended its own.
  if(m_grab)
  {
    const double v = snap(
        InfiniteScroller::move(event),
        event->modifiers() & (Qt::AltModifier | Qt::ShiftModifier));
    if(v != m_value)
    {
      m_value = v;
      if(m_unit != Free && m_unit != Parent)
        m_values[slotOf(m_unit)] = valueAt(m_unit, v);
      update();
    }
    InfiniteScroller::stop(*this, event);
    m_grab = false;
    sliderReleased();
  }
  else if(event->button() == Qt::RightButton && m_unit == Free)
  {
    DefaultGraphicsKnobImpl::contextMenuEvent(*this, event->scenePos());
  }
  event->accept();
}

void QGraphicsGridChooser::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  // Free: the right-click types the time in. Otherwise whatever is below
  // handles it.
  event->setAccepted(m_unit == Free);
}

//! QEvent::UngrabMouse: the scene took the implicit grab away and there will be
//! no release to end the drag on.
bool QGraphicsGridChooser::sceneEvent(QEvent* event)
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

void QGraphicsGridChooser::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  // Quick clicks on the readout make every second one a double-click: it is
  // one more click there, not a reset.
  if(event->button() != Qt::LeftButton || onReadout(event->pos()))
  {
    mousePressEvent(event);
    return;
  }

  setValue(init);
  m_grab = true;
  sliderMoved();
  sliderReleased();
  m_grab = false;

  update();
  event->accept();
}
}
