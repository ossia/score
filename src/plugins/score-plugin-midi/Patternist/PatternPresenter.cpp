#include "PatternPresenter.hpp"

#include <Process/Dataflow/NodeItem.hpp>
#include <Process/Focus/FocusDispatcher.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/Bind.hpp>

#include <Patternist/Commands/PatternProperties.hpp>
#include <Patternist/PatternController.hpp>
#include <Patternist/PatternParsing.hpp>
#include <Patternist/PatternTimeGrid.hpp>
#include <Patternist/PatternView.hpp>

namespace Patternist
{

Presenter::Presenter(
    const Patternist::ProcessModel& layer, View* view, const Process::Context& ctx,
    QObject* parent)
    : LayerPresenter{layer, view, ctx, parent}
    , m_view{view}
    , m_controller{new Controller{layer, ctx, this}}
{
  putToFront();

  // In a node the grid has the node's box to itself; in an interval it is laid
  // out on time, like every other layer.
  m_view->setMode(
      qobject_cast<Process::NodeItem*>(parent) ? View::Mode::Fit : View::Mode::Timeline);

  m_controller->attach(*m_view);
  // The grid takes every click, so nothing else would focus the layer, and the
  // layer menu only opens on the focused one.
  connect(m_view, &View::pressed, this, [this] {
    m_context.context.focusDispatcher.focus(this);
  });
  connect(m_view, &View::dropReceived, this, &Presenter::on_drop);

  auto grid = new TimeGridWatcher{layer, this};
  connect(grid, &TimeGridWatcher::changed, m_view, &View::setTimeGrid);
  m_view->setTimeGrid(grid->grid());
}

Presenter::~Presenter() { }

void Presenter::setWidth(qreal val, qreal defaultWidth)
{
  m_view->setWidth(val);
}

void Presenter::setHeight(qreal val)
{
  m_view->setHeight(val);
}

void Presenter::putToFront()
{
  m_view->setEnabled(true);
}

void Presenter::putBehind()
{
  m_view->setEnabled(false);
}

void Presenter::on_zoomRatioChanged(ZoomRatio zr)
{
  m_view->setZoom(zr);
}

void Presenter::parentGeometryChanged() { }

void Presenter::fillContextMenu(
    QMenu& menu, QPoint pos, QPointF scenepos, const Process::LayerContextMenuManager& m)
{
  LayerPresenter::fillContextMenu(menu, pos, scenepos, m);

  m_controller->fillMenu(menu, m_view->headerLaneAt(m_view->mapFromScene(scenepos)));
}

void Presenter::on_drop(const QPointF& pos, const QMimeData& md)
{
  auto patterns = parsePatternFiles(md);
  if(patterns.empty())
    return;
  if(patterns[0].empty())
    return;

  auto& model = static_cast<const Patternist::ProcessModel&>(this->model());
  CommandDispatcher<> disp{m_context.context.commandStack};
  disp.submit<UpdatePattern>(model, model.currentPattern(), patterns[0][0]);
}
}
