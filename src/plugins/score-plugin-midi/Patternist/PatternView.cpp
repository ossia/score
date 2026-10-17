#include "PatternView.hpp"

#include <score/graphics/widgets/QGraphicsNoteChooser.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/math.hpp>

#include <QFontMetricsF>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <Patternist/PatternEdit.hpp>
#include <Patternist/PatternModel.hpp>

#include <wobjectimpl.h>

#include <algorithm>
#include <cmath>
W_OBJECT_IMPL(Patternist::View)

namespace Patternist
{
namespace
{
constexpr double margin = 2;
constexpr double button_side = 14;
constexpr double numbers_height = 14;
//! Below this a step is too thin to aim at.
constexpr double min_editable_step = 3;

//! Steps of a pattern spread evenly over [x0; x1[, a beat being a quarter note.
std::vector<StepSlot> fitSlots(const Pattern& p, double x0, double x1)
{
  std::vector<StepSlot> res;
  if(p.length <= 0 || x1 <= x0)
    return res;
  const double w = (x1 - x0) / p.length;
  const int stepsPerBeat = p.division % 4 == 0 ? p.division / 4 : 0;
  res.reserve(p.length);
  for(int i = 0; i < p.length; i++)
  {
    const bool beat = stepsPerBeat > 0 && i % stepsPerBeat == 0;
    const bool bar = stepsPerBeat > 0 && i % (4 * stepsPerBeat) == 0;
    res.push_back({x0 + i * w, x0 + (i + 1) * w, i, false, bar, beat});
  }
  return res;
}
}

View::View(const Patternist::ProcessModel& model, QGraphicsItem* parent)
    : LayerView{parent}
    , m_model{model}
{
  setFlag(QGraphicsItem::ItemClipsToShape);
  setFlag(QGraphicsItem::ItemClipsChildrenToShape);
  setToolTip(
      tr("Pattern sequencer\n"
         "Click: add or remove a note · Shift-click: tie to the previous step\n"
         "Right-click on a note: remove it · Drag: paint over several steps\n"
         "Drag a lane name up or down: change its note"));

  con(model, &Patternist::ProcessModel::patternsChanged, this, [this] {
    invalidateSlots();
    updateLanes();
  });
  con(model, &Patternist::ProcessModel::currentPatternChanged, this, [this] {
    invalidateSlots();
    updateLanes();
  });
  con(model, &Patternist::ProcessModel::laneLabelsChanged, this,
      [this] { updateLanes(); });
  con(model, &Patternist::ProcessModel::execPosition, this, [this](int x) {
    if(x != m_execPosition)
    {
      m_execPosition = x;
      update();
    }
  });
  con(model, &Patternist::ProcessModel::execPattern, this, [this](int cur, int next) {
    if(cur != m_execPattern || next != m_pendingPattern)
    {
      m_execPattern = cur;
      m_pendingPattern = next;
      update();
    }
  });

  updateLanes();
}

View::~View() { }

void View::setMode(Mode m)
{
  if(m != m_mode)
  {
    m_mode = m;
    invalidateSlots();
    layoutLanes();
    update();
  }
}

void View::setAdvanced(bool b)
{
  if(b != m_advanced)
  {
    m_advanced = b;
    invalidateSlots();
    layoutLanes();
    update();
  }
}

void View::setTimeGrid(const TimeGrid& g)
{
  if(!(g == m_grid))
  {
    m_grid = g;
    invalidateSlots();
    update();
  }
}

void View::setZoom(ZoomRatio z)
{
  if(z != m_zoom)
  {
    m_zoom = z;
    invalidateSlots();
    update();
  }
}

const Pattern* View::pattern() const noexcept
{
  if(!ossia::valid_index(m_model.currentPattern(), m_model.patterns()))
    return nullptr;
  return &m_model.patterns()[m_model.currentPattern()];
}

void View::invalidateSlots()
{
  m_slotsValid = false;
}

const std::vector<StepSlot>& View::slots() const
{
  if(!m_slotsValid)
  {
    m_slots.clear();
    if(auto p = pattern())
    {
      if(m_mode == Mode::Timeline)
        m_slots = timelineSlots(*p, m_grid, m_zoom, width());
      else
        m_slots = fitSlots(*p, headerWidth(), width() - margin);
    }
    m_slotsValid = true;
  }
  return m_slots;
}

void View::updateLanes()
{
  auto p = pattern();
  if(!p)
    return;

  if(p->lanes.size() != m_lanes.size())
  {
    for(auto l : m_lanes)
      delete l;
    m_lanes.clear();

    for(std::size_t lane = 0; lane < p->lanes.size(); lane++)
    {
      auto sl = new score::QGraphicsNoteChooser{this};
      connect(sl, &score::QGraphicsNoteChooser::sliderMoved, this, [this, sl, lane] {
        noteChanged(lane, sl->value());
      });
      connect(sl, &score::QGraphicsNoteChooser::sliderReleased, this, [this] {
        noteChangeFinished();
      });
      m_lanes.push_back(sl);
    }
  }

  const auto labels = m_model.laneLabels();
  const bool solo = anySolo(*p);
  for(std::size_t lane = 0; lane < p->lanes.size(); lane++)
  {
    const Lane& l = p->lanes[lane];
    m_lanes[lane]->setFormatter([labels](int n) { return laneLabel(n, labels); });
    m_lanes[lane]->setValue(l.note);
    m_lanes[lane]->setOpacity(l.mute || (solo && !l.solo) ? 0.5 : 1.);
  }

  invalidateSlots();
  layoutLanes();
  update();
}

double View::headerWidth() const noexcept
{
  auto p = pattern();
  if(!p)
    return 0.;

  const QFontMetricsF metrics{score::Skin::instance().MonoFontSmall};
  double text = 0.;
  for(const Lane& l : p->lanes)
    text = std::max(
        text, metrics.horizontalAdvance(laneLabel(l.note, m_model.laneLabels())));
  double w = std::clamp(text + 8., 28., 96.);
  if(m_advanced)
    w += 2. * (button_side + 2.);
  return w;
}

double View::gridTop() const noexcept
{
  return m_advanced ? numbers_height : margin;
}

double View::laneHeight() const noexcept
{
  auto p = pattern();
  if(!p || p->lanes.empty())
    return 0.;
  return std::max(0., height() - gridTop() - margin) / p->lanes.size();
}

QRectF View::laneRect(int lane) const noexcept
{
  const double h = laneHeight();
  return {0., gridTop() + lane * h, width(), h};
}

QRectF View::muteRect(int lane) const noexcept
{
  const QRectF r = laneRect(lane);
  const double side = std::min(button_side, r.height() - 2.);
  const double x = headerWidth() - 2. * (button_side + 2.);
  return {x, r.center().y() - side / 2., side, side};
}

QRectF View::soloRect(int lane) const noexcept
{
  return muteRect(lane).translated(button_side + 2., 0.);
}

void View::layoutLanes()
{
  const double hw = headerWidth() - (m_advanced ? 2. * (button_side + 2.) : 0.);
  const double h = laneHeight();
  for(std::size_t lane = 0; lane < m_lanes.size(); lane++)
  {
    const QRectF r = laneRect(lane);
    const double ch = std::min(h, 16.);
    m_lanes[lane]->setSize({std::max(0., hw - 6.), ch});
    m_lanes[lane]->setPos(4., r.center().y() - ch / 2.);
  }
}

void View::heightChanged(qreal)
{
  layoutLanes();
  update();
}

void View::widthChanged(qreal)
{
  invalidateSlots();
  update();
}

bool View::canEdit() const noexcept
{
  auto p = pattern();
  if(!p)
    return false;
  if(m_mode == Mode::Timeline)
  {
    if(m_zoom <= 0.)
      return false;
    const double unit = 4. / std::max(1, p->division) * ossia::quarter_duration<double>;
    return unit / m_zoom >= min_editable_step;
  }
  return true;
}

View::Cell View::cellAt(QPointF pos) const noexcept
{
  auto p = pattern();
  if(!p || !canEdit())
    return {};
  const double h = laneHeight();
  if(h <= 0.)
    return {};
  const int lane = std::floor((pos.y() - gridTop()) / h);
  if(lane < 0 || lane >= int(p->lanes.size()))
    return {};

  // The header sits over the start of the timeline: it wins there.
  if(pos.x() < headerWidth())
    return {};

  const auto& s = slots();
  auto it
      = std::upper_bound(s.begin(), s.end(), pos.x(), [](double x, const StepSlot& sl) {
    return x < sl.x0;
  });
  if(it == s.begin())
    return {};
  --it;
  if(pos.x() >= it->x1)
    return {};
  return {lane, it->step};
}

int View::headerLaneAt(QPointF pos) const noexcept
{
  auto p = pattern();
  const double h = laneHeight();
  if(!p || h <= 0. || pos.x() < 0. || pos.x() >= headerWidth())
    return -1;
  const int lane = std::floor((pos.y() - gridTop()) / h);
  return ossia::valid_index(lane, p->lanes) ? lane : -1;
}

void View::paint_impl(QPainter* painter) const
{
  auto p = pattern();
  if(!p)
    return;

  paintCells(painter, *p);
  paintHeader(painter, *p);
  paintStatus(painter);
}

void View::paintCells(QPainter* painter, const Pattern& p) const
{
  auto& skin = score::Skin::instance();
  const auto& s = slots();
  const double h = laneHeight();
  if(h <= 0.)
    return;

  const bool playingThis = m_execPattern == m_model.currentPattern();
  const bool solo = anySolo(p);
  const double gapY = std::clamp(h * 0.12, h > 4. ? 1. : 0., 4.);

  for(int lane = 0; lane < int(p.lanes.size()); lane++)
  {
    const Lane& l = p.lanes[lane];
    const QRectF lr = laneRect(lane);
    const bool audible = !(l.mute || (solo && !l.solo));
    painter->setOpacity(audible ? 1. : 0.35);

    for(const StepSlot& sl : s)
    {
      const double w = sl.x1 - sl.x0;
      const double gapX = w > 4. ? 1. : 0.;
      const Note n
          = ossia::valid_index(sl.step, l.pattern) ? l.pattern[sl.step] : Note::Rest;
      const bool playing = playingThis && !sl.repeat && sl.step == m_execPosition;

      QRectF r{sl.x0, lr.y(), w - gapX, h - gapY};
      switch(n)
      {
        case Note::Rest:
          if(!sl.repeat)
            painter->fillRect(
                r, playing ? skin.Emphasis2.lighter.brush : skin.Emphasis2.main.brush);
          break;
        case Note::Legato:
          r.adjust(-gapX, 0., 0., 0.);
          [[fallthrough]];
        case Note::Note:
          painter->fillRect(
              r, sl.repeat ? skin.Base4.darker.brush
                 : playing ? skin.Base4.lighter.brush
                           : skin.Base4.main.brush);
          break;
      }
    }
  }
  painter->setOpacity(1.);

  // Bar and beat lines, then what plays and what the mouse is on.
  const double top = gridTop();
  const double bottom = top + h * p.lanes.size();
  for(const StepSlot& sl : s)
  {
    if(sl.barStart && sl.x0 > 0.)
    {
      painter->setPen(skin.LightGray.main.pen1);
      painter->drawLine(QPointF{sl.x0 - 0.5, top}, QPointF{sl.x0 - 0.5, bottom});
    }
    else if(sl.beatStart && sl.x0 > 0. && sl.x1 - sl.x0 >= min_editable_step)
    {
      painter->setPen(skin.Gray.main.pen1);
      painter->drawLine(QPointF{sl.x0 - 0.5, top}, QPointF{sl.x0 - 0.5, bottom});
    }

    if(playingThis && !sl.repeat && sl.step == m_execPosition)
      painter->fillRect(
          QRectF{sl.x0, top, sl.x1 - sl.x0, bottom - top}, QColor{255, 255, 255, 64});
  }

  if(m_hover.lane >= 0)
  {
    painter->setPen(skin.Light.main.pen1);
    painter->setBrush(Qt::NoBrush);
    const QRectF lr = laneRect(m_hover.lane);
    for(const StepSlot& sl : s)
      if(sl.step == m_hover.step && !sl.repeat)
        painter->drawRect(QRectF{sl.x0, lr.y(), sl.x1 - sl.x0 - 1., h - gapY - 1.});
  }

  if(m_advanced)
  {
    painter->setFont(skin.MonoFontSmall);
    painter->setPen(skin.Gray.lighter.pen1);
    for(const StepSlot& sl : s)
      if(sl.beatStart || sl.x1 - sl.x0 >= 16.)
        painter->drawText(
            QRectF{sl.x0, 0., std::max(16., sl.x1 - sl.x0), numbers_height},
            Qt::AlignLeft | Qt::AlignVCenter, QString::number(sl.step + 1));
  }
}

void View::paintHeader(QPainter* painter, const Pattern& p) const
{
  auto& skin = score::Skin::instance();
  const double hw = headerWidth();
  const double h = laneHeight();

  // Over the timeline the header covers the first steps: let them show through.
  QColor bg = skin.Background1.main.brush.color();
  if(m_mode == Mode::Timeline)
    bg.setAlpha(200);
  painter->fillRect(QRectF{0., 0., hw, height()}, bg);

  painter->setFont(skin.MonoFontSmall);
  for(int lane = 0; lane < int(p.lanes.size()); lane++)
  {
    const Lane& l = p.lanes[lane];
    if(m_advanced)
    {
      const auto drawButton
          = [&](QRectF r, const QString& text, bool on, const QBrush& onBrush) {
        painter->setPen(skin.Gray.main.pen1);
        painter->setBrush(on ? onBrush : skin.Background2.main.brush);
        painter->drawRect(r);
        painter->setPen(on ? skin.Dark.main.pen1 : skin.Gray.lighter.pen1);
        painter->drawText(r, Qt::AlignCenter, text);
      };
      drawButton(muteRect(lane), tr("M"), l.mute, skin.Warn3.main.brush);
      drawButton(soloRect(lane), tr("S"), l.solo, skin.Base2.main.brush);
    }
    else if(h >= 10. && (l.mute || l.solo))
    {
      const QRectF lr = laneRect(lane);
      painter->setPen(l.solo ? skin.Base2.main.pen1 : skin.Warn3.main.pen1);
      painter->drawText(
          QRectF{hw - 12., lr.y(), 10., h}, Qt::AlignCenter, l.solo ? tr("S") : tr("M"));
    }
  }
}

void View::paintStatus(QPainter* painter) const
{
  QString text;
  if(m_mode == Mode::Timeline && !canEdit() && pattern())
    text = tr("Zoom in to edit steps");
  else if(m_execPattern >= 0 && m_execPattern != m_model.currentPattern())
    text = tr("Playing pattern %1").arg(m_execPattern);
  if(m_pendingPattern >= 0 && m_execPattern >= 0)
    text += (text.isEmpty() ? QString{} : QStringLiteral(" · "))
            + tr("next: pattern %1").arg(m_pendingPattern);
  if(text.isEmpty())
    return;

  auto& skin = score::Skin::instance();
  painter->setFont(skin.SansFontSmall);
  const QFontMetricsF metrics{skin.SansFontSmall};
  const QRectF r{
      headerWidth() + 4., gridTop(), metrics.horizontalAdvance(text) + 8.,
      metrics.height() + 2.};
  QColor bg = skin.Background1.main.brush.color();
  bg.setAlpha(220);
  painter->fillRect(r, bg);
  painter->setPen(skin.Light.main.pen1);
  painter->drawText(r, Qt::AlignCenter, text);
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  pressed(event->pos());
  m_swallowContextMenu = false;

  auto p = pattern();
  if(!p)
    return;

  if(m_advanced && event->button() == Qt::LeftButton)
  {
    if(int lane = headerLaneAt(event->pos()); lane >= 0)
    {
      if(muteRect(lane).contains(event->pos()))
        return muteToggled(lane);
      if(soloRect(lane).contains(event->pos()))
        return soloToggled(lane);
    }
  }

  const Cell c = cellAt(event->pos());
  if(c.lane < 0)
    return;

  const Note current = p->lanes[c.lane].pattern[c.step];
  if(event->button() == Qt::LeftButton)
  {
    if(event->modifiers() & Qt::ShiftModifier)
      m_painting = p->lanes[c.lane].note < 128 ? Note::Legato : Note::Note;
    else
      m_painting = toggledNote(current);
  }
  else if(event->button() == Qt::RightButton && current != Note::Rest)
  {
    m_painting = Note::Rest;
    m_swallowContextMenu = true;
  }
  else
  {
    return;
  }

  m_lastPainted = c;
  paintStep(c.lane, c.step, *m_painting);
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(m_painting)
    paintAt(event->pos());
}

void View::paintAt(QPointF pos)
{
  const Cell c = cellAt(pos);
  if(c.lane < 0 || (c.lane == m_lastPainted.lane && c.step == m_lastPainted.step))
    return;
  m_lastPainted = c;
  paintStep(c.lane, c.step, *m_painting);
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  released(event->pos());
  endPainting();
}

void View::endPainting()
{
  if(m_painting)
  {
    m_painting.reset();
    m_lastPainted = {};
    paintFinished();
  }
}

//! A button-less move makes the scene drop the grab without any release: the
//! painting has to end there too, or the next press would extend it.
bool View::sceneEvent(QEvent* event)
{
  if(event->type() == QEvent::UngrabMouse)
    endPainting();
  return LayerView::sceneEvent(event);
}

void View::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  const Cell c = cellAt(event->pos());
  if(c.lane != m_hover.lane || c.step != m_hover.step)
  {
    m_hover = c;
    update();
  }
}

void View::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  LayerView::hoverLeaveEvent(event);
  if(m_hover.lane >= 0)
  {
    m_hover = {};
    update();
  }
}

void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  if(m_swallowContextMenu)
  {
    m_swallowContextMenu = false;
    event->accept();
    return;
  }
  LayerView::contextMenuEvent(event);
}

void View::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->pos(), *event->mimeData());
  event->accept();
}
}
