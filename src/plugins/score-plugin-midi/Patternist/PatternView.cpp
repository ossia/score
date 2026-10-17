#include "PatternView.hpp"

#include <score/graphics/widgets/QGraphicsNoteChooser.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/flicks.hpp>
#include <ossia/detail/math.hpp>

#include <QApplication>
#include <QFontMetricsF>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <Patternist/PatternModel.hpp>

#include <cmath>
#include <random>
#include <wobjectimpl.h>

#include <algorithm>
W_OBJECT_IMPL(Patternist::View)

namespace Patternist
{
namespace
{
constexpr double margin = 2;
constexpr double button_side = 14;
//! The editor's buttons, and the room above its name for the step numbers.
constexpr double button_w = 24;
constexpr double button_h = 18;
constexpr double min_button_h = 12;
//! The editor scrolls rather than make the lanes lower than this.
constexpr double min_lane_height = 20;
//! With rings: the lane cards at their tallest, and the room for step numbers.
constexpr double ring_row_height = 76;
constexpr double ring_numbers = 16;
constexpr double name_h = 20;
constexpr double numbers_height = 18;
//! Room between the header's edge and what it holds.
constexpr double pad = 6;
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
//! over it. Repeats keep the cells' tones and dim their notes; what plays is
//! tinted towards the notes' hue.
//!
//! Derived from the skin once, and again only when another skin loads.
struct Palette
{
  static const Palette& instance()
  {
    static Palette p;
    auto& skin = score::Skin::instance();
    if(p.m_loadIndex != skin.LoadIndex)
      p.reload(skin);
    return p;
  }

  void reload(const score::Skin& skin)
  {
    m_loadIndex = skin.LoadIndex;
    const QColor bg = skin.Background1.main.brush.color();
    const QColor base = skin.Emphasis2.main.brush.color();
    const QColor accent = skin.Base4.main.brush.color();
    const QColor light = skin.Light.main.brush.color();
    cell[0] = base;
    cell[1] = mix(bg, base, 0.6);
    playingCell = mix(base, accent, 0.22);
    note = accent;
    repeatNote = mix(bg, accent, 0.45);
    playingNote = mix(accent, light, 0.35);
    selectedLane = mix(base, light, 0.06);
    hover = mix(base, light, 0.55);
    selection = skin.Base2.main.brush.color();
    header = mix(bg, base, 0.45);
    selectedHeader = mix(header, light, 0.08);
    gap = bg;
    fieldBackground = skin.Background2.main.brush.color();
    // A step without a note keeps its value, but it plays nothing: barely
    // off the background.
    restBar = mix(fieldBackground, accent, 0.1);
    fieldOrigin = mix(fieldBackground, base, 0.6);

    small = skin.MonoFontSmall;
    editorSmall = skin.Medium8Pt;
    editorSmall.setPixelSize(11);
  }
  QColor cell[2];
  QColor playingCell;
  QColor note;
  QColor repeatNote;
  QColor playingNote;
  QColor selectedLane;
  QColor hover;
  QColor selection;
  QColor header;
  QColor selectedHeader;
  QColor gap;
  QColor fieldBackground;
  QColor restBar;
  QColor fieldOrigin;
  QFont small;
  QFont editorSmall;

private:
  int m_loadIndex{-1};
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
         "Click: add or remove a note\n"
         "Shift-click: tie to the previous step\n"
         "Right-click on a note: remove it\n"
         "Drag: paint over several steps\n"
         "Drag a lane name up or down: change its note\n"
         "In the parameter bars: Shift-drag moves them all, Alt-drag blends them "
         "towards a random state"));

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
  con(model, &Patternist::ProcessModel::execStepsChanged, this, [this] { update(); });
  // Colours and fonts come from the skin: another one remeasures and repaints.
  connect(&score::Skin::instance(), &score::Skin::changed, this, [this] { updateLanes(); });
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
  m_selectionValid = false;
  m_headerWidth.reset();
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
      else if(ringsMode())
      {
        // Each ring is its own loop; the parameter bars spread its steps.
        const auto sig = signatureAt(m_grid, m_grid.playStart);
        for(const Lane& l : p->lanes)
        {
          const int len = laneLength(*p, l), div = laneDivision(*p, l);
          m_slots.push_back(fitLaneSlots(
              len, div, sig, 4. * len / std::max(1, div), headerWidth(),
              width() - margin));
        }
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

QFont View::nameFont() const
{
  // The editor's names read like its toolbar.
  return m_advanced ? QApplication::font() : Palette::instance().small;
}

const QFont& View::smallFont() const
{
  const auto& p = Palette::instance();
  return m_advanced ? p.editorSmall : p.small;
}

//! Measures every lane's name: worked out once, then kept until the lanes,
//! their names or the size change.
double View::headerWidth() const noexcept
{
  if(!m_headerWidth)
    m_headerWidth = measureHeader();
  return *m_headerWidth;
}

double View::measureHeader() const noexcept
{
  auto p = pattern();
  if(!p)
    return 0.;
  if(!m_advanced && laneHeight() < min_label_height)
    return margin;

  const QFontMetricsF metrics{nameFont()};
  double text = 0.;
  for(const Lane& l : p->lanes)
    text = std::max(
        text, metrics.horizontalAdvance(laneLabel(l.note, m_model.laneLabels())));
  if(!m_advanced)
    return std::clamp(text + 8., 28., 96.);

  // The name above its buttons, or beside them when the lanes are low; and
  // the parameter buttons under the lanes.
  const QFontMetricsF small{smallFont()};
  double fields = 0.;
  for(auto f : allFields)
    fields = std::max(fields, small.horizontalAdvance(fieldName(f)) + 12.);
  fields = std::max(
      fields, small.horizontalAdvance(tr("Random")) + small.horizontalAdvance(tr("Reset"))
                  + 26.);
  // The lane's steps and rate side by side, at their widest.
  fields = std::max(
      fields, small.horizontalAdvance(tr("%1 steps").arg(128))
                  + small.horizontalAdvance(QStringLiteral("1/64")) + 2. * 8. + 4.);
  const double buttons = 2. * button_w + 2.;
  const double header
      = stackedHeader() ? std::max(text, buttons) : text + buttons + pad;
  return std::clamp(std::max(header, fields) + 2. * pad, 110., 200.);
}

bool View::stackedHeader() const noexcept
{
  return laneHeight() >= name_h + button_h + 10.;
}

//! Room for a third row in the header: the lane's own steps and rate.
bool View::laneValues() const noexcept
{
  return m_advanced && laneHeight() >= name_h + 2. * button_h + 16.;
}

double View::cellHeight() const noexcept
{
  const double h = laneHeight();
  return h - std::clamp(h * 0.12, h > 4. ? 1. : 0., 4.);
}

double View::gridTop() const noexcept
{
  return m_advanced ? numbers_height : margin;
}

double View::fieldHeight() const noexcept
{
  if(!m_advanced)
    return 0.;
  // Room for a button per parameter, and a row for Random and Reset, the
  // buttons getting smaller before the bars do.
  const double rows = std::size(allFields) + 1;
  const double minimum = rows * min_button_h + 8.;
  return std::clamp(height() * 0.3, minimum, std::max(minimum, 200.));
}

double View::fieldButtonHeight() const noexcept
{
  const double rows = std::size(allFields) + 1;
  return std::clamp((fieldHeight() - 8.) / rows, min_button_h, button_h);
}

double View::minimumHeight() const noexcept
{
  auto p = pattern();
  const int lanes = p ? int(p->lanes.size()) : 0;
  return gridTop() + lanes * min_lane_height + 8. + fieldHeight() + margin;
}

bool View::laneButtons() const noexcept
{
  return laneHeight() >= min_button_h + 2.;
}

QRectF View::fieldRect() const noexcept
{
  if(!m_advanced)
    return {};
  const double h = fieldHeight();
  const double x = headerWidth();
  return {x, height() - margin - h, width() - margin - x, h};
}

double View::gridBottom() const noexcept
{
  return m_advanced ? height() - margin - fieldHeight() - 8. : height() - margin;
}

double View::laneHeight() const noexcept
{
  auto p = pattern();
  if(!p || p->lanes.empty())
    return 0.;
  const double h = std::max(0., gridBottom() - gridTop()) / p->lanes.size();
  // With rings, the headers are the same cards as on the grid, stacked from
  // the top: as tall as a name, its buttons, its steps and rate, no taller.
  return ringsMode() ? std::min(h, ring_row_height) : h;
}

bool View::ringsMode() const noexcept
{
  return m_rings && m_advanced && m_mode == Mode::Fit;
}

void View::setRings(bool b)
{
  if(b != m_rings)
  {
    m_rings = b;
    m_selectionArea = {};
    invalidateSlots();
    layoutLanes();
    update();
  }
}

QRectF View::ringArea() const noexcept
{
  const double x = headerWidth() + pad;
  return {x, gridTop(), std::max(0., width() - margin - x), std::max(0., gridBottom() - gridTop())};
}

QPointF View::ringCenter() const noexcept
{
  return ringArea().center();
}

//! Lane 0 outermost, each ring as thick as the others but the selected one,
//! twice as thick; the step numbers of the selected lane go around them all.
View::Ring View::ring(int lane) const noexcept
{
  auto p = pattern();
  const int lanes = p ? int(p->lanes.size()) : 0;
  const QRectF area = ringArea();
  const double outer = std::max(0., std::min(area.width(), area.height()) / 2. - ring_numbers);
  const double hole = std::max(outer * 0.3, std::min(outer, 36.));
  const double unit = (outer - hole) / std::max(1, lanes + 1);
  double r = outer;
  for(int i = 0; i < lanes; i++)
  {
    const double t = unit * (i == m_selectedLane ? 2. : 1.);
    if(i == lane)
      return {r - t + (t > 6. ? 1.5 : 0.), r};
    r -= t;
  }
  return {hole, hole};
}

//! Where a slot of a lane falls on its ring, in turns from the top, clockwise.
std::pair<double, double> View::slotTurns(const StepSlot& sl) const noexcept
{
  const double x0 = headerWidth(), w = std::max(1., width() - margin - x0);
  return {(sl.x0 - x0) / w, (sl.x1 - x0) / w};
}

View::Cell View::ringCellAt(QPointF pos) const noexcept
{
  auto p = pattern();
  if(!p)
    return {};
  const QPointF d = pos - ringCenter();
  const double radius = std::hypot(d.x(), d.y());
  double turn = std::atan2(d.x(), -d.y()) / (2. * M_PI);
  if(turn < 0.)
    turn += 1.;
  for(int lane = 0; lane < int(p->lanes.size()); lane++)
  {
    const Ring r = ring(lane);
    if(radius < r.inner || radius >= r.outer)
      continue;
    for(const StepSlot& sl : slots(lane))
    {
      const auto [a, b] = slotTurns(sl);
      if(turn >= a && turn < b)
        return {lane, sl.step};
    }
  }
  return {};
}

QRectF View::laneRect(int lane) const noexcept
{
  const double h = laneHeight();
  return {0., gridTop() + lane * h, width(), h};
}

QRectF View::nameRect(int lane) const noexcept
{
  const QRectF r = laneRect(lane);
  const double hw = headerWidth();
  if(!m_advanced)
  {
    const double ch = std::min(r.height(), 16.);
    return {4., r.center().y() - ch / 2., std::max(0., hw - 4.), ch};
  }
  if(stackedHeader())
  {
    // The name and the rows under it, as one block in the lane's card.
    const double block
        = name_h + 4. + button_h + (laneValues() ? 4. + button_h : 0.);
    const double top = r.y() + (cellHeight() - block) / 2.;
    return {pad, top, hw - 2. * pad, name_h};
  }
  const double ch = std::min(r.height(), name_h);
  return {
      pad, r.center().y() - ch / 2., std::max(0., hw - 3. * pad - 2. * button_w - 2.),
      ch};
}

QRectF View::muteRect(int lane) const noexcept
{
  const QRectF r = laneRect(lane);
  if(stackedHeader())
  {
    const QRectF name = nameRect(lane);
    return {pad, name.bottom() + 4., button_w, button_h};
  }
  const double side = std::min(button_h, r.height() - 2.);
  const double x = headerWidth() - pad - 2. * button_w - 2.;
  return {x, r.center().y() - side / 2., button_w, side};
}

QRectF View::soloRect(int lane) const noexcept
{
  return muteRect(lane).translated(button_w + 2., 0.);
}

//! The lane's steps (0) or rate (1), under its mute and solo buttons.
QRectF View::valueRect(int lane, int which) const noexcept
{
  const QRectF m = muteRect(lane);
  const double w = (headerWidth() - 2. * pad - 4.) / 2.;
  return {pad + which * (w + 4.), m.bottom() + 4., w, button_h};
}

QRectF View::fieldButtonRect(int i) const noexcept
{
  const QRectF area = fieldRect();
  const double w = headerWidth() - 2. * pad;
  const double h = fieldButtonHeight();
  const int n = std::size(allFields);
  if(i < n)
    return {pad, area.top() + i * h, w, h - 2.};
  // Random, then Reset, side by side under the parameters.
  const double half = (w - 2.) / 2.;
  return {pad + (i - n) * (half + 2.), area.top() + n * h + 4., half, h - 2.};
}

int View::buttonAt(QPointF pos) const noexcept
{
  auto p = pattern();
  if(!p || !m_advanced || pos.x() >= headerWidth())
    return -1;
  if(pos.y() < gridBottom())
  {
    const int lane = headerLaneAt(pos);
    if(lane < 0)
      return -1;
    if(!laneButtons())
      return -1;
    if(muteRect(lane).contains(pos))
      return mute_button + lane;
    if(soloRect(lane).contains(pos))
      return solo_button + lane;
    if(laneValues())
      for(int which : {0, 1})
        if(valueRect(lane, which).contains(pos))
          return (which == 0 ? steps_button : rate_button) + lane;
    return -1;
  }
  for(int i = 0; i < int(std::size(allFields)) + 2; i++)
    if(fieldButtonRect(i).contains(pos))
      return field_button + i;
  return -1;
}

void View::layoutLanes()
{
  const double h = laneHeight();
  const QFont font = nameFont();
  for(std::size_t lane = 0; lane < m_lanes.size(); lane++)
  {
    const QRectF r = nameRect(lane);
    m_lanes[lane]->setFont(font);
    m_lanes[lane]->setSize(r.size());
    m_lanes[lane]->setPos(r.topLeft());
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
  if(ringsMode())
    return pos.x() >= headerWidth() && pos.y() < gridBottom() ? ringCellAt(pos) : Cell{};
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

  if(ringsMode())
    paintRings(painter, *p);
  else
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

  const auto& colors = Palette::instance();
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

      // Selected steps are tinted wherever they repeat.
      const QColor& cellColor = isPlaying ? colors.playingCell : colors.cell[shade];
      painter->fillRect(
          cell, selection().contains(lane, sl.step) ? mix(cellColor, colors.selection, 0.25)
                                                    : cellColor);
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
        // Tied to the step before, as loud as the note it holds; a ratchet's
        // other hits are marked by gaps.
        const float vel = stepParams(l, struckStep(p, l, sl.step)).velocity;
        const double vh = cellH * (0.3 + 0.7 * vel);
        // Joined to the step before, it ends at its gate.
        const double gw = cell.width() * std::clamp(sp.gate, 0.05f, 1.f);
        const QRectF tie = QRectF{cell.x(), cell.bottom() - vh, gw, vh}.adjusted(
            -gapX - barGap, 0., 0., 0.);
        painter->fillRect(tie, noteColor);
        if(sp.ratchet > 1 && cell.width() >= 2. * sp.ratchet)
        {
          const double e = std::exp2(-2. * sp.pace);
          for(int k = 1; k < sp.ratchet; k++)
            painter->fillRect(
                QRectF{
                    cell.x() + std::pow(double(k) / sp.ratchet, e) * cell.width() - 1.,
                    cell.y(), 1., cellH},
                colors.gap);
        }
      }
      else
      {
        // Height is velocity, width is gate, position is timing, within the
        // cell: an early note is cut at the cell's start.
        const double vh = cellH * (0.3 + 0.7 * sp.velocity);
        const double gw = cell.width() * std::max(0.25f, sp.gate);
        const QRectF note
            = QRectF{cell.x() + sp.offset * w, cell.bottom() - vh, gw, vh}.intersected(
                cell);
        if(sp.ratchet <= 1 || note.width() < 2. * sp.ratchet)
        {
          painter->fillRect(note, noteColor);
        }
        else
        {
          // Spaced as the pace bends them.
          const double e = std::exp2(-2. * sp.pace);
          const auto at = [&](int k) {
            return note.x() + std::pow(double(k) / sp.ratchet, e) * note.width();
          };
          for(int k = 0; k < sp.ratchet; k++)
            painter->fillRect(
                QRectF{at(k), note.y(), std::max(1., at(k + 1) - at(k) - 1.), vh},
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
    painter->setFont(smallFont());
    for(const StepSlot& sl : slots(m_selectedLane))
    {
      if(!(sl.beatStart || sl.x1 - sl.x0 >= 20.))
        continue;
      painter->setPen(sl.repeat ? skin.Gray.darker.pen1 : skin.Gray.lighter.pen1);
      painter->drawText(
          QRectF{sl.x0 + 2., 0., std::max(20., sl.x1 - sl.x0), numbers_height},
          Qt::AlignLeft | Qt::AlignVCenter, QString::number(sl.step + 1));
    }
  }
}

void View::paintButton(
    QPainter* painter, QRectF r, const QString& text, bool on, const QColor& onColor,
    int id) const
{
  auto& skin = score::Skin::instance();
  const auto& colors = Palette::instance();
  const bool hovered = id == m_hoverButton;
  painter->setPen(Qt::NoPen);
  painter->setBrush(
      on        ? onColor
      : hovered ? colors.hover
                : colors.cell[0]);
  painter->drawRoundedRect(r, 2., 2.);
  painter->setPen(on || hovered ? skin.Dark.main.pen1 : skin.Gray.lighter.pen1);
  painter->drawText(r, Qt::AlignCenter, text);
}

//! An annular sector from turn a to turn b (clockwise from the top), between
//! two radii.
static QPainterPath sector(QPointF c, double inner, double outer, double a, double b)
{
  const QRectF ro{c.x() - outer, c.y() - outer, 2. * outer, 2. * outer};
  const QRectF ri{c.x() - inner, c.y() - inner, 2. * inner, 2. * inner};
  const double start = 90. - 360. * a, span = -360. * (b - a);
  QPainterPath path;
  path.arcMoveTo(ro, start);
  path.arcTo(ro, start, span);
  path.arcTo(ri, start + span, -span);
  path.closeSubpath();
  return path;
}

void View::paintRings(QPainter* painter, const Pattern& p) const
{
  auto& skin = score::Skin::instance();
  const auto& colors = Palette::instance();
  const QPointF c = ringCenter();
  const bool solo = anySolo(p);
  painter->setRenderHint(QPainter::Antialiasing, true);
  painter->setPen(Qt::NoPen);

  for(int lane = 0; lane < int(p.lanes.size()); lane++)
  {
    const Lane& l = p.lanes[lane];
    const Ring r = ring(lane);
    const double thick = r.outer - r.inner;
    if(thick <= 0.5)
      continue;
    const bool selected = lane == m_selectedLane;
    const bool audible = !(l.mute || (solo && !l.solo));
    // The selected ring stands out: the others step back.
    const double laneOpacity = (audible ? 1. : 0.35) * (selected ? 1. : 0.7);
    const auto& s = slots(lane);
    const int playing = playingSlot(lane);
    const double mid = (r.inner + r.outer) / 2.;
    // A pixel of gap between cells, wider where a bar starts.
    const double gap = 1. / (2. * M_PI * mid);

    for(int i = 0; i < int(s.size()); i++)
    {
      const StepSlot& sl = s[i];
      auto [a, b] = slotTurns(sl);
      a += (sl.barStart && i > 0 ? 2.5 : 0.5) * gap;
      b -= 0.5 * gap;
      if(b <= a)
        continue;
      const bool isPlaying = i == playing;
      painter->setOpacity(laneOpacity);
      painter->fillPath(
          sector(c, r.inner, r.outer, a, b),
          isPlaying ? colors.playingCell : colors.cell[sl.shade]);

      const Note n = writtenNote(p, l, sl.step);
      if(n == Note::Rest)
        continue;
      const StepParams& sp = stepParams(l, sl.step);
      const QColor& noteColor = isPlaying ? colors.playingNote : colors.note;
      painter->setOpacity(laneOpacity * (0.3 + 0.7 * sp.chance));
      // Outward is velocity, along the ring the gate, shifted by the timing.
      const double outer = r.inner + thick * (0.3 + 0.7 * sp.velocity);
      if(n == Note::Legato)
      {
        const float vel = stepParams(l, struckStep(p, l, sl.step)).velocity;
        painter->fillPath(
            sector(
                c, r.inner, r.inner + thick * (0.3 + 0.7 * vel), a - 1.5 * gap,
                a + (b - a) * std::clamp(sp.gate, 0.05f, 1.f)),
            noteColor);
        continue;
      }
      const double w = b - a;
      const double na = std::clamp(a + sp.offset * w, a, b);
      const double nb = std::clamp(na + w * std::max(0.25f, sp.gate), a, b);
      painter->fillPath(sector(c, r.inner, outer, na, nb), noteColor);
      if(sp.ratchet > 1 && (nb - na) * 2. * M_PI * mid >= 2. * sp.ratchet)
      {
        const double e = std::exp2(-2. * sp.pace);
        painter->setOpacity(laneOpacity);
        for(int k = 1; k < sp.ratchet; k++)
        {
          const double t = na + std::pow(double(k) / sp.ratchet, e) * (nb - na);
          painter->fillPath(sector(c, r.inner, outer, t - 0.5 * gap, t + 0.5 * gap), colors.gap);
        }
      }
      if(sp.condition != Condition::Always && thick >= 6.)
      {
        const double t = (na + nb) / 2. * 2. * M_PI;
        const QPointF dot{
            c.x() + std::sin(t) * (r.outer - 2.5), c.y() - std::cos(t) * (r.outer - 2.5)};
        painter->setOpacity(laneOpacity);
        painter->setBrush(colors.hover);
        painter->drawEllipse(dot, 1.8, 1.8);
        painter->setBrush(Qt::NoBrush);
      }
    }
  }
  painter->setOpacity(1.);

  // The selected lane: an outline around its ring, its step numbers around
  // all of them, its name in the middle.
  if(ossia::valid_index(m_selectedLane, p.lanes))
  {
    const Lane& l = p.lanes[m_selectedLane];
    const Ring r = ring(m_selectedLane);
    painter->setPen(QPen{colors.hover, 1.5});
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(c, r.outer + 1., r.outer + 1.);
    painter->drawEllipse(c, r.inner - 1., r.inner - 1.);

    const double outermost = ring(0).outer;
    painter->setFont(smallFont());
    const QFontMetricsF fm{smallFont()};
    const auto& s = slots(m_selectedLane);
    const double perStep = 2. * M_PI * outermost / std::max<std::size_t>(1, s.size());
    for(const StepSlot& sl : s)
    {
      // Every step when there is room, the beats otherwise.
      if(!(perStep >= 18. || sl.beatStart))
        continue;
      const auto [a, b] = slotTurns(sl);
      const double t = (a + b) / 2. * 2. * M_PI;
      const double rr = outermost + ring_numbers / 2. + 1.;
      const QPointF at{c.x() + std::sin(t) * rr, c.y() - std::cos(t) * rr};
      painter->setPen(skin.Gray.lighter.pen1);
      painter->drawText(
          QRectF{at.x() - 14., at.y() - fm.height() / 2., 28., fm.height()},
          Qt::AlignCenter, QString::number(sl.step + 1));
    }

    const double hole = std::max(0., ring(int(p.lanes.size())).inner - 4.);
    if(hole > 20.)
      paintRingCenter(painter, p, QRectF{c.x() - hole, c.y() - hole, 2. * hole, 2. * hole});
  }

  if(m_hover.lane >= 0)
  {
    const Ring r = ring(m_hover.lane);
    painter->setPen(QPen{colors.hover, 1.});
    painter->setBrush(Qt::NoBrush);
    for(const StepSlot& sl : slots(m_hover.lane))
      if(sl.step == m_hover.step)
      {
        const auto [a, b] = slotTurns(sl);
        painter->drawPath(sector(c, r.inner + 0.5, r.outer - 0.5, a, b));
      }
  }
  painter->setRenderHint(QPainter::Antialiasing, false);
}

//! While playing, the notes struck on the step each lane is on; stopped, the
//! selected lane's name and length.
void View::paintRingCenter(QPainter* painter, const Pattern& p, QRectF box) const
{
  auto& skin = score::Skin::instance();
  const bool playing = m_execPattern == m_model.currentPattern() && m_execPattern >= 0
                       && !m_model.execSteps().empty();
  if(playing)
  {
    QStringList struck;
    const bool solo = anySolo(p);
    for(int lane = 0; lane < int(p.lanes.size()); lane++)
    {
      const int slot = playingSlot(lane);
      const auto& s = slots(lane);
      if(slot < 0 || slot >= int(s.size()))
        continue;
      const Lane& l = p.lanes[lane];
      if(audibleNote(p, l, s[slot].step, solo) == Note::Note)
        struck.push_back(laneLabel(l.note, m_model.laneLabels()));
    }
    const QFontMetricsF fm{nameFont()};
    const int fit = std::max(1, int(box.height() * 0.8 / fm.height()));
    if(struck.size() > fit)
    {
      const int more = int(struck.size()) - (fit - 1);
      struck = struck.mid(0, fit - 1);
      struck.push_back(tr("%1 more").arg(more));
    }
    painter->setFont(nameFont());
    painter->setPen(skin.Light.main.pen1);
    painter->drawText(box, Qt::AlignCenter, struck.join(QLatin1Char('\n')));
    return;
  }

  if(!ossia::valid_index(m_selectedLane, p.lanes))
    return;
  const Lane& l = p.lanes[m_selectedLane];
  const double hole = box.height() / 2.;
  int hits = 0;
  const int len = laneLength(p, l);
  for(int i = 0; i < len; i++)
    hits += writtenNote(p, l, i) != Note::Rest;
  painter->setPen(skin.Light.main.pen1);
  painter->setFont(nameFont());
  painter->drawText(
      box.adjusted(0., 0., 0., -hole * 0.35), Qt::AlignCenter,
      QFontMetricsF{nameFont()}.elidedText(
          laneLabel(l.note, m_model.laneLabels()), Qt::ElideRight, box.width() - 8.));
  painter->setFont(smallFont());
  painter->setPen(skin.Gray.lighter.pen1);
  painter->drawText(
      box.adjusted(0., hole * 0.35, 0., 0.), Qt::AlignCenter,
      tr("%1 hits, %2 steps, 1/%3").arg(hits).arg(len).arg(laneDivision(p, l)));
}

void View::paintValue(
    QPainter* painter, QRectF r, const QString& text, bool inherited, int id) const
{
  auto& skin = score::Skin::instance();
  const auto& colors = Palette::instance();
  const bool active = id == m_hoverButton || (m_valueDrag && m_valueDrag->id == id);
  painter->setPen(Qt::NoPen);
  painter->setBrush(active ? mix(colors.cell[0], colors.hover, 0.4) : colors.cell[0]);
  painter->drawRoundedRect(r, 2., 2.);
  painter->setPen(inherited ? skin.Gray.main.pen1 : skin.Light.main.pen1);
  painter->drawText(
      r.adjusted(3., 0., -3., 0.), Qt::AlignCenter,
      QFontMetricsF{painter->font()}.elidedText(text, Qt::ElideRight, r.width() - 6.));
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

  // Each lane's header is a card on its row, the start of the lane: same
  // height, same gaps, lit with it when selected.
  const auto& colors = Palette::instance();
  const double ch = cellHeight();
  painter->setFont(smallFont());
  for(int lane = 0; lane < int(p.lanes.size()); lane++)
  {
    const Lane& l = p.lanes[lane];
    const QRectF lr = laneRect(lane);
    const bool selected = lane == m_selectedLane;
    painter->fillRect(
        QRectF{0., lr.y(), hw - 2., ch},
        selected ? colors.selectedHeader : colors.header);
    if(selected)
      painter->fillRect(QRectF{0., lr.y(), 3., ch}, skin.Base4.main.brush);

    // Too low to aim at: mute and solo stay in the lane's menu.
    if(!laneButtons())
      continue;
    painter->setRenderHint(QPainter::Antialiasing, true);
    paintButton(
        painter, muteRect(lane), tr("M"), l.mute, skin.Warn3.main.brush.color(),
        mute_button + lane);
    paintButton(
        painter, soloRect(lane), tr("S"), l.solo, skin.Base2.main.brush.color(),
        solo_button + lane);
    if(laneValues())
    {
      // Dimmed while the lane follows the pattern.
      const int len = laneLength(p, l), div = laneDivision(p, l);
      paintValue(
          painter, valueRect(lane, 0), tr("%1 steps").arg(len), l.length == 0,
          steps_button + lane);
      paintValue(
          painter, valueRect(lane, 1), QStringLiteral("1/%1").arg(div), l.division == 0,
          rate_button + lane);
    }
    painter->setRenderHint(QPainter::Antialiasing, false);
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

  const auto& colors = Palette::instance();
  painter->fillRect(area, colors.fieldBackground);

  // The parameters to choose from, beside their bars, on a card like the
  // lanes' headers.
  painter->fillRect(
      QRectF{0., area.top(), area.left() - 2., area.height()}, colors.header);
  painter->setFont(smallFont());
  painter->setRenderHint(QPainter::Antialiasing, true);
  for(int i = 0; i < int(std::size(allFields)); i++)
    paintButton(
        painter, fieldButtonRect(i), fieldName(allFields[i]), allFields[i] == m_field,
        skin.Base4.main.brush.color(), field_button + i);
  const int n = std::size(allFields);
  paintButton(painter, fieldButtonRect(n), tr("Random"), false, {}, field_button + n);
  paintButton(
      painter, fieldButtonRect(n + 1), tr("Reset"), false, {}, field_button + n + 1);
  painter->setRenderHint(QPainter::Antialiasing, false);

  const auto range = fieldRange(m_field);
  const auto toY = [&](float v) {
    const double t = (v - range.min) / (range.max - range.min);
    return area.bottom() - t * area.height();
  };
  const double originY = toY(range.origin);
  if(range.origin > range.min)
  {
    painter->setPen(QPen{colors.fieldOrigin, 1.});
    painter->drawLine(QPointF{area.left(), originY}, QPointF{area.right(), originY});
  }

  for(const StepSlot& sl : slots(m_selectedLane))
  {
    const double w = sl.x1 - sl.x0;
    const double inset = w > 4. ? 1. : 0.;
    const float v = stepField(stepParams(l, sl.step), m_field);
    const double y = toY(v);
    // A tie that only holds the note before plays no velocity of its own.
    const Note note = writtenNote(p, l, sl.step);
    const bool rest
        = note == Note::Rest
          || (note == Note::Legato
              && (m_field == StepField::Velocity || m_field == StepField::VelocityRandom)
              && !tieStrikes(p, l, sl.step));
    const QRectF bar{
        QPointF{sl.x0 + inset, std::min(y, originY)},
        QPointF{sl.x1 - inset, std::max(y, originY) + 1.}};
    painter->fillRect(
        bar, rest ? colors.restBar : sl.repeat ? colors.repeatNote : colors.note);
    const QString text = fieldText(m_field, v);
    if(!sl.repeat && QFontMetricsF{painter->font()}.horizontalAdvance(text) + 2. <= w)
    {
      painter->setPen(skin.Light.main.pen1);
      painter->drawText(QRectF{sl.x0, area.y(), w, 16.}, Qt::AlignCenter, text);
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
    text += (text.isEmpty() ? QString{} : QStringLiteral(", "))
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
    if(const int b = buttonAt(event->pos()); b >= steps_button)
    {
      // Dragged up or down; it starts from what the lane plays now.
      const int lane = b % 1000;
      const Lane& l = p->lanes[lane];
      const bool steps = b < rate_button;
      m_valueDrag = ValueDrag{
          b, lane, steps ? 0 : 1, event->pos().y(),
          steps ? laneLength(*p, l) : laneDivision(*p, l)};
      setSelectedLane(lane);
      laneSelected(lane);
      update();
      return;
    }
    if(const int b = buttonAt(event->pos()); b >= 0)
      return pressButton(b, event->modifiers());
    if(int lane = headerLaneAt(event->pos()); lane >= 0)
    {
      setSelectedLane(lane);
      laneSelected(lane);
      return;
    }
    if(fieldRect().contains(event->pos()))
    {
      if(event->modifiers() & (Qt::ShiftModifier | Qt::AltModifier))
        return startFieldDrag(event->pos(), event->modifiers());
      m_editingField = true;
      editFieldAt(event->pos());
      return;
    }
  }

  const Cell c = cellAt(event->pos());
  if(c.lane < 0)
  {
    clearSelection();
    return;
  }

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

  // Selecting: in the select mode, or with Ctrl.
  if(m_advanced && !ringsMode() && event->button() == Qt::LeftButton
     && (m_selectMode || (event->modifiers() & Qt::ControlModifier)))
  {
    m_selectAnchor = event->pos();
    m_selecting = true;
    selectOver(event->pos());
    return;
  }
  clearSelection();

  const Lane& l = p->lanes[c.lane];
  const Note current = writtenNote(*p, l, c.step);
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

void View::pressButton(int id, Qt::KeyboardModifiers mods)
{
  const int n = std::size(allFields);
  if(id >= field_button)
  {
    const int i = id - field_button;
    if(i < n)
    {
      setField(allFields[i]);
      fieldSelected(int(m_field));
    }
    else
    {
      // Shift applies to every lane.
      const int lane = (mods & Qt::ShiftModifier) ? -1 : m_selectedLane;
      if(i == n)
        fieldRandomized(lane, int(m_field));
      else
        fieldReset(lane, int(m_field));
    }
  }
  else if(id >= solo_button)
    soloToggled(id - solo_button);
  else if(id >= mute_button)
    muteToggled(id - mute_button);
}

const StepSelection& View::selection() const
{
  if(!m_selectionValid)
  {
    m_selection = {};
    auto p = pattern();
    if(p && m_selectionArea.lane0 >= 0)
    {
      m_selection.lane0 = m_selectionArea.lane0;
      for(int lane = m_selectionArea.lane0;
          lane <= m_selectionArea.lane1 && lane < int(p->lanes.size()); lane++)
      {
        // The steps whose middle is in the area, once each.
        std::vector<int> steps;
        for(const StepSlot& sl : slots(lane))
        {
          const double mid = (sl.x0 + sl.x1) / 2.;
          if(mid < m_selectionArea.x0 || mid >= m_selectionArea.x1)
            continue;
          if(std::find(steps.begin(), steps.end(), sl.step) != steps.end())
            break;
          steps.push_back(sl.step);
        }
        m_selection.steps.push_back(std::move(steps));
      }
    }
    m_selectionValid = true;
  }
  return m_selection;
}

void View::setSelectionArea(SelectionArea a)
{
  if(!(a == m_selectionArea))
  {
    m_selectionArea = a;
    m_selectionValid = false;
    update();
    selectionChanged();
  }
}

void View::clearSelection()
{
  setSelectionArea({});
}

void View::selectAll()
{
  if(auto p = pattern(); p && !p->lanes.empty())
    setSelectionArea({0, int(p->lanes.size()) - 1, headerWidth(), width()});
}

//! The lanes from the anchor's to the one at `pos`, over the steps between
//! theirs: the same stretch of time in every lane.
void View::selectOver(QPointF pos)
{
  const auto under = [this](QPointF pt) -> std::optional<std::pair<int, StepSlot>> {
    const Cell c = cellAt(pt);
    if(c.lane < 0)
      return std::nullopt;
    const auto& s = slots(c.lane);
    auto it = std::find_if(s.begin(), s.end(), [&](const StepSlot& sl) {
      return pt.x() >= sl.x0 && pt.x() < sl.x1;
    });
    if(it == s.end())
      return std::nullopt;
    return std::make_pair(c.lane, *it);
  };
  const auto a = under(m_selectAnchor);
  const auto b = under(pos);
  if(!a || !b)
    return;
  setSelectionArea(
      {std::min(a->first, b->first), std::max(a->first, b->first),
       std::min(a->second.x0, b->second.x0), std::max(a->second.x1, b->second.x1)});
}

View::PasteTarget View::pasteTarget(int lanes) const
{
  PasteTarget t;
  double x = headerWidth();
  if(m_hover.lane >= 0)
  {
    t.lane = m_hover.lane;
    x = m_hoverX;
  }
  else if(m_selectionArea.lane0 >= 0)
  {
    t.lane = m_selectionArea.lane0;
    x = m_selectionArea.x0;
  }
  else
  {
    t.lane = std::max(0, m_selectedLane);
  }

  // Each lane from its step at the same time as the target's.
  for(int r = 0; r < lanes; r++)
  {
    int step = -1;
    for(const StepSlot& sl : slots(t.lane + r))
    {
      if(sl.x1 > x + 0.5)
      {
        step = sl.step;
        break;
      }
    }
    t.starts.push_back(step);
  }
  return t;
}

void View::setSelectMode(bool b)
{
  m_selectMode = b;
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(m_valueDrag)
  {
    const auto& d = *m_valueDrag;
    const int v = d.start + int(std::round((d.y - event->pos().y()) / 6.));
    laneValueEdited(
        d.lane, d.which,
        std::clamp(v, 1, d.which == 0 ? Pattern::maxLength : Pattern::maxDivision));
  }
  else if(m_editingField)
    editFieldAt(event->pos());
  else if(m_fieldDrag != FieldDrag::None)
    dragField(event->pos());
  else if(m_painting)
    paintAt(event->pos());
  else if(m_selecting)
    selectOver(event->pos());
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

//! Shift moves every bar by the same amount; Alt picks a random state and
//! blends towards it the further the mouse goes. Both act on the selected
//! steps of the lane when it has some, on all its steps otherwise.
void View::startFieldDrag(QPointF pos, Qt::KeyboardModifiers mods)
{
  auto p = pattern();
  if(!p || !ossia::valid_index(m_selectedLane, p->lanes))
    return;
  const Lane& l = p->lanes[m_selectedLane];
  m_fieldDrag = (mods & Qt::ShiftModifier) ? FieldDrag::Offset : FieldDrag::Random;
  m_dragOrigin = pos;
  m_dragBase.resize(l.params.size());
  m_dragTarget.resize(l.params.size());
  for(std::size_t i = 0; i < l.params.size(); i++)
    m_dragBase[i] = stepField(l.params[i], m_field);

  std::mt19937 rng{std::random_device{}()};
  for(auto& v : m_dragTarget)
    v = anyFieldValue(m_field, rng);

  m_dragSteps.clear();
  const auto& sel = selection();
  if(sel.valid() && m_selectedLane >= sel.lane0
     && m_selectedLane < sel.lane0 + int(sel.steps.size()))
    m_dragSteps = sel.steps[m_selectedLane - sel.lane0];
  if(m_dragSteps.empty())
    for(int i = 0; i < std::min<int>(laneLength(*p, l), l.params.size()); i++)
      m_dragSteps.push_back(i);
}

void View::dragField(QPointF pos)
{
  const QRectF area = fieldRect();
  if(area.height() <= 0.)
    return;
  const auto range = fieldRange(m_field);
  auto& values = m_dragValues;
  values = m_dragBase;

  if(m_fieldDrag == FieldDrag::Offset)
  {
    const float delta
        = (m_dragOrigin.y() - pos.y()) / area.height() * (range.max - range.min);
    for(int i : m_dragSteps)
      if(i < std::ssize(values))
        values[i] = std::clamp(m_dragBase[i] + delta, range.min, range.max);
  }
  else
  {
    const QPointF d = pos - m_dragOrigin;
    const float t = std::clamp(
        float(std::hypot(d.x(), d.y()) / std::max(80., area.height())), 0.f, 1.f);
    for(int i : m_dragSteps)
      if(i < std::ssize(values))
        values[i] = m_dragBase[i] + t * (m_dragTarget[i] - m_dragBase[i]);
  }
  fieldValuesEdited(m_selectedLane, int(m_field));
}

//! On a lane's steps or rate: back to following the pattern.
void View::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  const int b = buttonAt(event->pos());
  if(b >= steps_button && event->button() == Qt::LeftButton)
  {
    event->accept();
    laneValueEdited(b % 1000, b < rate_button ? 0 : 1, 0);
    laneValueEditFinished();
    return;
  }
  LayerView::mouseDoubleClickEvent(event);
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  released(event->pos());
  endPainting();
}

void View::endPainting()
{
  m_selecting = false;
  if(m_valueDrag)
  {
    m_valueDrag.reset();
    laneValueEditFinished();
    update();
  }
  if(m_editingField || m_fieldDrag != FieldDrag::None)
  {
    m_editingField = false;
    m_fieldDrag = FieldDrag::None;
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
  const int b = buttonAt(event->pos());
  if(c.lane >= 0)
    for(const StepSlot& sl : slots(c.lane))
      if(event->pos().x() >= sl.x0 && event->pos().x() < sl.x1)
        m_hoverX = sl.x0;
  if(c.lane != m_hover.lane || c.step != m_hover.step || b != m_hoverButton)
  {
    m_hover = c;
    m_hoverButton = b;
    update();
  }
}

void View::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  LayerView::hoverLeaveEvent(event);
  if(m_hover.lane >= 0 || m_hoverButton >= 0)
  {
    m_hover = {};
    m_hoverButton = -1;
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
  // opens on the focused layer: ask once the focus is through. The press also
  // grabbed the mouse, which the menu needs to highlight what it hovers.
  event->accept();
  endPainting();
  if(scene() && scene()->mouseGrabberItem() == this)
    ungrabMouse();
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
