#include "PatternView.hpp"

#include <Process/Style/ScenarioStyle.hpp>

#include <score/graphics/GraphicWidgets.hpp>
#include <score/graphics/widgets/QGraphicsNoteChooser.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/Cursor.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QPainter>

#include <algorithm>
#include <cmath>

#include <Patternist/PatternModel.hpp>

#include <score_plugin_midi_export.h>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Patternist::View)

namespace Patternist
{

View::View(const Patternist::ProcessModel& model, QGraphicsItem* parent)
    : LayerView{parent}
    , m_model{model}
{
  setFlag(QGraphicsItem::ItemClipsToShape);
  setFlag(QGraphicsItem::ItemClipsChildrenToShape);
  con(model, &Patternist::ProcessModel::patternsChanged, this,
      [this] { updateLanes(); });
  con(model, &Patternist::ProcessModel::currentPatternChanged, this,
      [this] { updateLanes(); });
  con(model, &Patternist::ProcessModel::execPosition, this, [this](int x) {
    m_execPosition = x;
    update();
  });

  updateLanes();
}

View::~View() { }

namespace
{
//! Room on the left of the grid for the note choosers.
constexpr double header_width = 42;
constexpr double margin = 2;
constexpr double max_spacing = 4;

//! The grid stretches over the whole layer: lanes share its height, steps
//! share what the note choosers leave of its width.
struct Grid
{
  Grid(const Pattern& p, double width, double height) noexcept
      : lanes{int(p.lanes.size())}
      , steps{p.length}
  {
    if(lanes > 0)
      laneHeight = std::max(0., height - 2. * margin) / lanes;
    if(steps > 0)
      stepWidth = std::max(0., width - header_width - margin) / steps;
    spacing = std::min({max_spacing, laneHeight * 0.2, stepWidth * 0.2});
  }

  QRectF cell(int lane, int step) const noexcept
  {
    return {
        header_width + step * stepWidth, margin + lane * laneHeight,
        stepWidth - spacing, laneHeight - spacing};
  }

  //! Fills the gap with the previous step, so that a tied note reads as one.
  QRectF legato(int lane, int step) const noexcept
  {
    const QRectF c = cell(lane, step);
    return {c.x() - spacing, c.y() + c.height() * 0.1, spacing, c.height() * 0.8};
  }

  //! Lane and step under a point, or {-1, -1}: the gaps belong to the cell
  //! before them so that a click never falls through.
  std::pair<int, int> at(QPointF pos) const noexcept
  {
    if(laneHeight <= 0. || stepWidth <= 0.)
      return {-1, -1};
    const int lane = std::floor((pos.y() - margin) / laneHeight);
    const int step = std::floor((pos.x() - header_width) / stepWidth);
    if(lane < 0 || lane >= lanes || step < 0 || step >= steps)
      return {-1, -1};
    return {lane, step};
  }

  int lanes{};
  int steps{};
  double laneHeight{};
  double stepWidth{};
  double spacing{};
};
}

void View::updateLanes()
{
  if(!ossia::valid_index(m_model.currentPattern(), m_model.patterns()))
    return;
  auto& cur_p = m_model.patterns()[m_model.currentPattern()];
  if(cur_p.lanes.size() != m_lanes.size())
  {
    for(auto l : m_lanes)
      delete l;
    m_lanes.clear();

    for(std::size_t lane = 0; lane < cur_p.lanes.size(); lane++)
    {
      auto sl = new score::QGraphicsNoteChooser{this};

      sl->setValue(cur_p.lanes[lane].note);

      connect(sl, &score::QGraphicsNoteChooser::sliderMoved, this, [this, sl, lane] {
        noteChanged(lane, sl->value());
      });
      connect(sl, &score::QGraphicsNoteChooser::sliderReleased, this, [this] {
        noteChangeFinished();
      });
      m_lanes.push_back(sl);
    }
  }
  else
  {
    for(std::size_t lane = 0; lane < cur_p.lanes.size(); lane++)
      m_lanes[lane]->setValue(cur_p.lanes[lane].note);
  }

  layoutLanes();
  update();
}

void View::heightChanged(qreal)
{
  layoutLanes();
  update();
}

void View::widthChanged(qreal)
{
  update();
}

void View::layoutLanes()
{
  if(!ossia::valid_index(m_model.currentPattern(), m_model.patterns()))
    return;
  const Grid grid{m_model.patterns()[m_model.currentPattern()], width(), height()};
  for(std::size_t lane = 0; lane < m_lanes.size(); lane++)
  {
    const QRectF cell = grid.cell(lane, 0);
    const double h = static_cast<QGraphicsItem*>(m_lanes[lane])->boundingRect().height();
    m_lanes[lane]->setPos(10, std::max(cell.y(), cell.center().y() - h / 2.));
  }
}

void View::paint_impl(QPainter* painter) const
{
  if(!ossia::valid_index(m_model.currentPattern(), m_model.patterns()))
    return;

  auto& style = score::Skin::instance();
  painter->setPen(style.Base4.main.pen1);

  auto& cur_p = m_model.patterns()[m_model.currentPattern()];
  const Grid grid{cur_p, width(), height()};
  for(int lane = 0; lane < grid.lanes; lane++)
  {
    auto& l = cur_p.lanes[lane];

    for(int i = 0; i < grid.steps; i++)
    {
      // A lane can be shorter than the pattern when it was loaded from a file
      const Note n = ossia::valid_index(i, l.pattern) ? l.pattern[i] : Note::Rest;
      switch(n)
      {
        case Note::Rest:
          painter->setBrush(
              (i != m_execPosition) ? style.Emphasis2.main.brush
                                    : style.Emphasis2.lighter.brush);
          painter->drawRect(grid.cell(lane, i));
          break;
        case Note::Legato:
          painter->setBrush(
              (i != m_execPosition) ? style.Base4 : style.Base4.lighter.brush);
          painter->drawRect(grid.legato(lane, i));
          painter->drawRect(grid.cell(lane, i));
          break;
        case Note::Note:
          painter->setBrush(
              (i != m_execPosition) ? style.Base4 : style.Base4.lighter.brush);
          painter->drawRect(grid.cell(lane, i));
          break;
      }
    }
  }
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if(!ossia::valid_index(m_model.currentPattern(), m_model.patterns()))
    return;

  const Grid grid{m_model.patterns()[m_model.currentPattern()], width(), height()};
  if(auto [lane, step] = grid.at(event->pos()); lane >= 0)
    toggled(lane, step);
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void View::dropEvent(QGraphicsSceneDragDropEvent* event)
{
  dropReceived(event->pos(), *event->mimeData());
  event->accept();
}

}
