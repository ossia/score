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
#include <QPainterPath>
#include <QTimer>

#include <Patternist/PatternModel.hpp>

#include <cmath>
#include <wobjectimpl.h>

#include <algorithm>
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
//! Below this the lane names are hidden.
constexpr double min_label_height = 10;
//! Below this the lanes of a node stop lining up in time.
constexpr double min_aligned_step = 6;

QColor mix(const QColor& a, const QColor& b, double t) noexcept
{
  return QColor::fromRgbF(
      a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
      a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

//! Shades of the step cells. Beats alternate between two close tones and bars
//! are told apart by a wider gap, so that the grid reads without lines drawn
//! over it; repeats and what plays shift towards the background and the
//! notes' hue.
struct Palette
{
  explicit Palette(const score::Skin& skin)
  {
    const QColor bg = skin.Background1.main.brush.color();
    const QColor base = skin.Emphasis2.main.brush.color();
    const QColor accent = skin.Base4.main.brush.color();
    cell[0] = base;
    cell[1] = mix(bg, base, 0.6);
    repeatCell[0] = mix(bg, base, 0.55);
    repeatCell[1] = mix(bg, base, 0.35);
    playingCell = mix(base, accent, 0.22);
    note = accent;
    repeatNote = mix(bg, accent, 0.45);
    playingNote = mix(accent, skin.Light.main.brush.color(), 0.35);
    selectedLane = mix(base, skin.Light.main.brush.color(), 0.06);
    hover = mix(base, skin.Light.main.brush.color(), 0.55);
    gap = bg;
  }
  QColor cell[2];
  QColor repeatCell[2];
  QColor playingCell;
  QColor note;
  QColor repeatNote;
  QColor playingNote;
  QColor selectedLane;
  QColor hover;
  QColor gap;
};
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
    m_execPosition = x;
    update();
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

void View::setField(StepField f)
{
  m_field = f;
  update();
}

void View::setSelectedLane(int lane)
{
  if(lane != m_selectedLane)
  {
    m_selectedLane = lane;
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

const std::vector<StepSlot>& View::slots(int lane) const
{
  if(!m_slotsValid)
  {
    m_slots.clear();
    m_span = 0.;
    if(auto p = pattern())
    {
      if(m_mode == Mode::Timeline)
      {
        for(const Lane& l : p->lanes)
          m_slots.push_back(timelineSlots(
              laneLength(*p, l), laneDivision(*p, l), m_grid, m_zoom, width()));
      }
      else
      {
        auto layout = fitLayout(
            *p, signatureAt(m_grid, m_grid.playStart), headerWidth(),
            width() - margin, min_aligned_step);
        m_slots = std::move(layout.lanes);
        m_span = layout.span;
      }
    }
    m_slotsValid = true;
  }
  static const std::vector<StepSlot> none;
  return ossia::valid_index(lane, m_slots) ? m_slots[lane] : none;
}

int View::playingSlot(int lane) const noexcept
{
  auto p = pattern();
  if(!p || m_execPattern != m_model.currentPattern() || m_execPattern < 0
     || !ossia::valid_index(lane, p->lanes))
    return -1;
  const auto& steps = m_model.execSteps();
  if(!ossia::valid_index(lane, steps))
    return -1;
  const auto& s = slots(lane);
  if(m_mode == Mode::Timeline)
    return slotWithIndex(s, steps[lane]);
  const Lane& l = p->lanes[lane];
  return Patternist::playingSlot(
      s, steps[lane], laneLength(*p, l), laneDivision(*p, l), m_span);
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
      if(scene())
        sl->installSceneEventFilter(this);
      connect(sl, &score::QGraphicsNoteChooser::sliderMoved, this, [this, sl, lane] {
        noteChanged(lane, sl->value());
      });
      connect(sl, &score::QGraphicsNoteChooser::sliderReleased, this, [this] {
        noteChangeFinished();
      });
      m_lanes.push_back(sl);
    }
  }
  if(m_selectedLane >= int(p->lanes.size()))
    m_selectedLane = int(p->lanes.size()) - 1;

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
  if(!m_advanced && laneHeight() < min_label_height)
    return margin;

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

QRectF View::fieldRect() const noexcept
{
  if(!m_advanced)
    return {};
  const double h = std::clamp(height() * 0.28, 40., 140.);
  const double x = headerWidth();
  return {x, height() - margin - h, width() - margin - x, h};
}

double View::gridBottom() const noexcept
{
  return m_advanced ? fieldRect().top() - 8. : height() - margin;
}

double View::laneHeight() const noexcept
{
  auto p = pattern();
  if(!p || p->lanes.empty())
    return 0.;
  return std::max(0., gridBottom() - gridTop()) / p->lanes.size();
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
    // Names that would run into each other are left out.
    m_lanes[lane]->setVisible(h >= min_label_height);
  }
}

void View::heightChanged(qreal)
{
  // The header narrows when the lanes get too thin for their names.
  invalidateSlots();
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
    int fastest = p->division;
    for(const Lane& l : p->lanes)
      fastest = std::max(fastest, laneDivision(*p, l));
    const double unit = 4. / std::max(1, fastest) * ossia::quarter_duration<double>;
    return unit / m_zoom >= min_editable_step;
  }
  return true;
}

static std::vector<StepSlot>::const_iterator
slotAt(const std::vector<StepSlot>& s, double x) noexcept
{
  auto it = std::upper_bound(
      s.begin(), s.end(), x, [](double x, const StepSlot& sl) { return x < sl.x0; });
  if(it == s.begin())
    return s.end();
  --it;
  return x < it->x1 ? it : s.end();
}

View::Cell View::cellAt(QPointF pos) const noexcept
{
  auto p = pattern();
  if(!p || !canEdit())
    return {};
  const double h = laneHeight();
  if(h <= 0. || pos.y() >= gridBottom())
    return {};
  const int lane = std::floor((pos.y() - gridTop()) / h);
  if(lane < 0 || lane >= int(p->lanes.size()))
    return {};

  // The header sits over the start of the timeline: it wins there.
  if(pos.x() < headerWidth())
    return {};

  const auto& s = slots(lane);
  auto it = slotAt(s, pos.x());
  if(it == s.end())
    return {};
  return {lane, it->step};
}

int View::fieldStepAt(QPointF pos) const noexcept
{
  const auto& s = slots(m_selectedLane);
  auto it = slotAt(s, pos.x());
  return it == s.end() ? -1 : it->step;
}

int View::headerLaneAt(QPointF pos) const noexcept
{
  auto p = pattern();
  const double h = laneHeight();
  if(!p || h <= 0. || pos.x() < 0. || pos.x() >= headerWidth()
     || pos.y() >= gridBottom())
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
  if(m_advanced)
    paintField(painter, *p);
  paintStatus(painter);
}

void View::paintCells(QPainter* painter, const Pattern& p) const
{
  auto& skin = score::Skin::instance();
  const double h = laneHeight();
  if(h <= 0.)
    return;

  const Palette colors{skin};
  const bool solo = anySolo(p);
  const double gapY = std::clamp(h * 0.12, h > 4. ? 1. : 0., 4.);
  const double cellH = h - gapY;
  const double hw = headerWidth();

  for(int lane = 0; lane < int(p.lanes.size()); lane++)
  {
    const Lane& l = p.lanes[lane];
    const auto& s = slots(lane);
    const QRectF lr = laneRect(lane);
    const bool audible = !(l.mute || (solo && !l.solo));
    const int playing = playingSlot(lane);

    if(m_advanced && lane == m_selectedLane)
      painter->fillRect(
          QRectF{hw, lr.y() - gapY / 2., width() - hw, h}, colors.selectedLane);

    painter->setOpacity(audible ? 1. : 0.35);
    for(int i = 0; i < int(s.size()); i++)
    {
      const StepSlot& sl = s[i];
      const double w = sl.x1 - sl.x0;
      // A bar opens with a wider gap than a step.
      const double gapX = w > 4. ? 1. : 0.;
      const double barGap
          = sl.barStart && sl.x0 > hw + 0.5 ? std::clamp(w * 0.2, 1., 3.) : 0.;
      const QRectF cell{sl.x0 + barGap, lr.y(), w - gapX - barGap, cellH};
      const bool isPlaying = i == playing;
      const int shade = sl.shade;

      painter->fillRect(
          cell, isPlaying   ? colors.playingCell
                : sl.repeat ? colors.repeatCell[shade]
                            : colors.cell[shade]);
      // Too thin for a gap: the bar line is the background showing through.
      if(barGap > 0. && w <= 4.)
        painter->fillRect(QRectF{sl.x0, lr.y(), 1., cellH}, colors.gap);

      const Note n = writtenNote(p, l, sl.step);
      if(n == Note::Rest)
        continue;

      const StepParams& sp = stepParams(l, sl.step);
      const QColor& noteColor = isPlaying   ? colors.playingNote
                                : sl.repeat ? colors.repeatNote
                                            : colors.note;
      // A note that may not play is faint.
      const double opacity = painter->opacity();
      painter->setOpacity(opacity * (0.3 + 0.7 * sp.chance));

      if(n == Note::Legato)
      {
        painter->fillRect(cell.adjusted(-gapX - barGap, 0., 0., 0.), noteColor);
      }
      else
      {
        // Height is velocity, width is gate, position is timing, within the
        // cell: an early note is cut at the cell's start and marked there.
        const double vh = cellH * (0.3 + 0.7 * sp.velocity);
        const double gw = cell.width() * std::max(0.25f, sp.gate);
        const QRectF note
            = QRectF{cell.x() + sp.offset * w, cell.bottom() - vh, gw, vh}.intersected(
                cell);
        if(sp.offset < 0.f && cell.width() >= 6.)
          painter->fillRect(
              QRectF{cell.x(), cell.top(), 1., cellH}, noteColor);
        if(sp.ratchet <= 1 || note.width() < 2. * sp.ratchet)
        {
          painter->fillRect(note, noteColor);
        }
        else
        {
          const double sub = note.width() / sp.ratchet;
          for(int k = 0; k < sp.ratchet; k++)
            painter->fillRect(
                QRectF{note.x() + k * sub, note.y(), std::max(1., sub - 1.), vh},
                noteColor);
        }
      }

      if(sp.condition != Condition::Always && w >= 6. && cellH >= 6.)
      {
        const double c = std::min({6., w * 0.4, cellH * 0.4});
        QPainterPath tri;
        tri.moveTo(cell.right() - c, cell.top());
        tri.lineTo(cell.right(), cell.top());
        tri.lineTo(cell.right(), cell.top() + c);
        tri.closeSubpath();
        painter->fillPath(tri, colors.hover);
      }
      painter->setOpacity(opacity);
    }
    painter->setOpacity(1.);
  }

  // The step under the mouse, wherever it repeats.
  if(m_hover.lane >= 0)
  {
    painter->setPen(QPen{colors.hover, 1.});
    painter->setBrush(Qt::NoBrush);
    const QRectF lr = laneRect(m_hover.lane);
    for(const StepSlot& sl : slots(m_hover.lane))
      if(sl.step == m_hover.step)
        painter->drawRect(QRectF{sl.x0 + 0.5, lr.y() + 0.5, sl.x1 - sl.x0 - 2., cellH - 1.});
  }

  if(m_advanced)
  {
    painter->setFont(skin.MonoFontSmall);
    for(const StepSlot& sl : slots(m_selectedLane))
    {
      if(!(sl.beatStart || sl.x1 - sl.x0 >= 16.))
        continue;
      painter->setPen(sl.repeat ? skin.Gray.darker.pen1 : skin.Gray.lighter.pen1);
      painter->drawText(
          QRectF{sl.x0, 0., std::max(16., sl.x1 - sl.x0), numbers_height},
          Qt::AlignLeft | Qt::AlignVCenter, QString::number(sl.step + 1));
    }
  }
}

void View::paintHeader(QPainter* painter, const Pattern& p) const
{
  auto& skin = score::Skin::instance();
  const double hw = headerWidth();

  // Over the timeline the header covers the first steps: let them show through.
  QColor bg = skin.Background1.main.brush.color();
  if(m_mode == Mode::Timeline)
    bg.setAlpha(200);
  painter->fillRect(QRectF{0., 0., hw, gridBottom()}, bg);

  if(!m_advanced)
    return;

  painter->setFont(skin.MonoFontSmall);
  for(int lane = 0; lane < int(p.lanes.size()); lane++)
  {
    const Lane& l = p.lanes[lane];
    if(lane == m_selectedLane)
    {
      const QRectF lr = laneRect(lane);
      painter->fillRect(QRectF{0., lr.y(), 2., lr.height() - 1.}, skin.Base4.main.brush);
    }

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
}

void View::paintField(QPainter* painter, const Pattern& p) const
{
  if(!ossia::valid_index(m_selectedLane, p.lanes))
    return;
  auto& skin = score::Skin::instance();
  const Lane& l = p.lanes[m_selectedLane];
  const QRectF area = fieldRect();
  if(area.height() <= 4.)
    return;

  const Palette colors{skin};
  painter->fillRect(area, skin.Background2.main.brush);
  painter->setFont(skin.MonoFontSmall);
  painter->setPen(skin.Gray.lighter.pen1);
  painter->drawText(
      QRectF{4., area.y(), area.x() - 6., 14.}, Qt::AlignLeft | Qt::AlignVCenter,
      fieldName(m_field));

  const auto range = fieldRange(m_field);
  const auto toY = [&](float v) {
    const double t = (v - range.min) / (range.max - range.min);
    return area.bottom() - t * area.height();
  };
  const double originY = toY(range.origin);
  if(range.origin > range.min)
  {
    painter->setPen(QPen{colors.cell[0], 1.});
    painter->drawLine(QPointF{area.left(), originY}, QPointF{area.right(), originY});
  }

  for(const StepSlot& sl : slots(m_selectedLane))
  {
    const double w = sl.x1 - sl.x0;
    const double inset = w > 4. ? 1. : 0.;
    const float v = stepField(stepParams(l, sl.step), m_field);
    const double y = toY(v);
    const bool rest = writtenNote(p, l, sl.step) == Note::Rest;
    const QRectF bar{
        QPointF{sl.x0 + inset, std::min(y, originY)},
        QPointF{sl.x1 - inset, std::max(y, originY) + 1.}};
    painter->fillRect(
        bar, rest || sl.repeat ? colors.repeatNote : colors.note);
    if(w >= 22. && !sl.repeat)
    {
      painter->setPen(skin.Light.main.pen1);
      painter->drawText(
          QRectF{sl.x0, area.y(), w, 14.}, Qt::AlignCenter, fieldText(m_field, v));
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
      setSelectedLane(lane);
      laneSelected(lane);
      return;
    }
    if(fieldRect().contains(event->pos()))
    {
      m_editingField = true;
      editFieldAt(event->pos());
      return;
    }
  }

  const Cell c = cellAt(event->pos());
  if(c.lane < 0)
    return;

  if(m_advanced && c.lane != m_selectedLane)
  {
    setSelectedLane(c.lane);
    laneSelected(c.lane);
  }

  if(m_advanced && event->button() == Qt::RightButton)
  {
    m_swallowContextMenu = true;
    stepMenuRequested(c.lane, c.step, event->screenPos());
    return;
  }

  const Lane& l = p->lanes[c.lane];
  // Over a rhythm that adds or masks, clicks edit the written steps beneath it.
  const bool written = l.euclid.enabled && l.euclid.mode != EuclidMode::Replace;
  const Note current
      = written
            ? (ossia::valid_index(c.step, l.pattern) ? l.pattern[c.step] : Note::Rest)
            : writtenNote(*p, l, c.step);
  if(event->button() == Qt::LeftButton)
  {
    if(event->modifiers() & Qt::ShiftModifier)
      m_painting = l.note < 128 ? Note::Legato : Note::Note;
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
  if(m_editingField)
    editFieldAt(event->pos());
  else if(m_painting)
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

void View::editFieldAt(QPointF pos)
{
  const QRectF area = fieldRect();
  const int step = fieldStepAt(pos);
  if(step < 0 || area.height() <= 0.)
    return;
  const auto range = fieldRange(m_field);
  const double t = std::clamp((area.bottom() - pos.y()) / area.height(), 0., 1.);
  fieldEdited(
      m_selectedLane, step, int(m_field),
      float(range.min + t * (range.max - range.min)));
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  released(event->pos());
  endPainting();
}

void View::endPainting()
{
  if(m_editingField)
  {
    m_editingField = false;
    fieldEditFinished();
  }
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

//! A scene event filter only takes when both items are in the same scene.
QVariant View::itemChange(GraphicsItemChange change, const QVariant& value)
{
  if(change == ItemSceneHasChanged && scene())
    for(auto l : m_lanes)
      l->installSceneEventFilter(this);
  return LayerView::itemChange(change, value);
}

//! The lane names are note choosers: pressing one also selects its lane.
bool View::sceneEventFilter(QGraphicsItem* watched, QEvent* event)
{
  if(m_advanced && event->type() == QEvent::GraphicsSceneMousePress)
  {
    auto it = std::find(m_lanes.begin(), m_lanes.end(), watched);
    if(it != m_lanes.end())
    {
      const int lane = int(it - m_lanes.begin());
      setSelectedLane(lane);
      laneSelected(lane);
    }
  }
  return false;
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
  // The press asked for focus through a queued connection, and the menu only
  // opens on the focused layer: ask once the focus is through.
  event->accept();
  QTimer::singleShot(
      0, this, [this, screen = event->screenPos(), scene = event->scenePos()] {
    askContextMenu(screen, scene);
  });
}

void View::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->pos(), *event->mimeData());
  event->accept();
}
}
