#include "PatternPresenter.hpp"

#include <Process/Dataflow/NodeItem.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/Bind.hpp>

#include <Patternist/Commands/PatternProperties.hpp>
#include <Patternist/PatternController.hpp>
#include <Patternist/PatternParsing.hpp>
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
  connect(m_view, &View::dropReceived, this, &Presenter::on_drop);

  con(layer, &ProcessModel::timeGridChanged, this, &Presenter::updateTimeGrid);
  con(layer, &ProcessModel::startOffsetChanged, this, &Presenter::updateTimeGrid);
  updateTimeGrid();
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

//! The engine reads its musical positions from the closest interval, from the
//! process's own upwards, that has a time signature or a tempo curve; without
//! one, from the root.
void Presenter::updateTimeGrid()
{
  auto& proc = static_cast<const Patternist::ProcessModel&>(model());

  TimeVal delta = TimeVal::zero();
  const Scenario::IntervalModel* found{};
  for(auto itv = Scenario::closestParentInterval(proc.parent()); itv;
      itv = Scenario::closestParentInterval(itv->parent()))
  {
    found = itv;
    if(itv->hasTimeSignature() || itv->tempoCurve())
      break;
    delta += itv->date();
  }

  if(found != m_metrics)
  {
    QObject::disconnect(m_metricsSignatures);
    QObject::disconnect(m_metricsHasSignature);
    m_metrics = found;
    if(found)
    {
      m_metricsSignatures = connect(
          found, &Scenario::IntervalModel::timeSignaturesChanged, this,
          &Presenter::updateTimeGrid);
      m_metricsHasSignature = connect(
          found, &Scenario::IntervalModel::hasTimeSignatureChanged, this,
          &Presenter::updateTimeGrid);
    }
  }

  TimeGrid g;
  if(found && found->hasTimeSignature())
    for(const auto& [date, sig] : found->timeSignatureMap())
      g.signatures.emplace_back(date, sig);
  g.playStart = delta;
  g.origin = (proc.flags() & Process::ProcessFlags::HandlesLooping)
                 ? delta
                 : delta - proc.startOffset();
  m_view->setTimeGrid(g);
}

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
