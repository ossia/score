#include "ClipLauncherPresenter.hpp"

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/ComponentUtils.hpp>
#include <score/tools/Bind.hpp>

#include <QMenu>
#include <QTimer>

#include <Scenario/Application/Drops/DropProcessInInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>

#include <ClipLauncher/Commands/AddCell.hpp>
#include <ClipLauncher/Commands/AddLane.hpp>
#include <ClipLauncher/Commands/AddScene.hpp>
#include <ClipLauncher/Commands/RemoveCell.hpp>
#include <ClipLauncher/Commands/RemoveLane.hpp>
#include <ClipLauncher/Commands/RemoveScene.hpp>
#include <ClipLauncher/Execution/ClipLauncherComponent.hpp>
#include <ClipLauncher/ProcessModel.hpp>
#include <ClipLauncher/View/ClipLauncherView.hpp>
W_OBJECT_IMPL(ClipLauncher::ClipLauncherPresenter)

namespace ClipLauncher
{

ClipLauncherPresenter::ClipLauncherPresenter(
    const ProcessModel& model, ClipLauncherView* view, const Process::Context& ctx,
    QObject* parent)
    : Process::LayerPresenter{model, view, ctx, parent}
    , m_model{model}
    , m_view{view}
{
  m_view->setModel(&m_model);

  connect(m_view, &ClipLauncherView::cellClicked, this, &ClipLauncherPresenter::on_cellClicked);
  connect(
      m_view, &ClipLauncherView::cellDoubleClicked, this,
      &ClipLauncherPresenter::on_cellDoubleClicked);
  connect(
      m_view, &ClipLauncherView::sceneLaunchClicked, this,
      &ClipLauncherPresenter::on_sceneLaunchClicked);
  connect(
      m_view, &ClipLauncherView::dropOnCell, this,
      &ClipLauncherPresenter::on_dropOnCell);
  connect(
      m_view, &ClipLauncherView::laneHeaderClicked, this,
      &ClipLauncherPresenter::on_laneHeaderClicked);
  connect(
      m_view, &ClipLauncherView::sceneHeaderClicked, this,
      &ClipLauncherPresenter::on_sceneHeaderClicked);

  // React to model changes (EntityMap uses Nano::Signal, not QObject)
  model.cells.added.connect<&ClipLauncherPresenter::on_cellChanged>(this);
  model.cells.removed.connect<&ClipLauncherPresenter::on_cellChanged>(this);
  model.lanes.added.connect<&ClipLauncherPresenter::on_laneChanged>(this);
  model.lanes.removed.connect<&ClipLauncherPresenter::on_laneChanged>(this);
  model.scenes.added.connect<&ClipLauncherPresenter::on_sceneChanged>(this);
  model.scenes.removed.connect<&ClipLauncherPresenter::on_sceneChanged>(this);

  // Progress update timer for live cell state feedback
  m_progressTimer = new QTimer{this};
  m_progressTimer->setInterval(33); // ~30fps
  connect(m_progressTimer, &QTimer::timeout, this, &ClipLauncherPresenter::updateView);
}

ClipLauncherPresenter::~ClipLauncherPresenter()
{
  // Disconnect Nano::Signal connections to prevent dangling callbacks
  m_model.cells.added.disconnect<&ClipLauncherPresenter::on_cellChanged>(this);
  m_model.cells.removed.disconnect<&ClipLauncherPresenter::on_cellChanged>(this);
  m_model.lanes.added.disconnect<&ClipLauncherPresenter::on_laneChanged>(this);
  m_model.lanes.removed.disconnect<&ClipLauncherPresenter::on_laneChanged>(this);
  m_model.scenes.added.disconnect<&ClipLauncherPresenter::on_sceneChanged>(this);
  m_model.scenes.removed.disconnect<&ClipLauncherPresenter::on_sceneChanged>(this);
}

const ProcessModel& ClipLauncherPresenter::model() const noexcept
{
  return m_model;
}

void ClipLauncherPresenter::setWidth(qreal width, qreal defaultWidth)
{
  m_view->setWidth(width);
}

void ClipLauncherPresenter::setHeight(qreal height)
{
  m_view->setHeight(height);
}

void ClipLauncherPresenter::putToFront()
{
  m_view->setVisible(true);
  m_progressTimer->start();
}

void ClipLauncherPresenter::putBehind()
{
  m_view->setVisible(false);
  m_progressTimer->stop();
}

void ClipLauncherPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  updateView();
}

void ClipLauncherPresenter::parentGeometryChanged()
{
  updateView();
}

Execution::ClipLauncherComponent* ClipLauncherPresenter::executionComponent() const
{
  return score::findComponent<Execution::ClipLauncherComponent>(m_model.components());
}

void ClipLauncherPresenter::on_cellClicked(int lane, int scene)
{
  auto* cell = m_model.cellAt(lane, scene);
  if(!cell)
    return;

  // During execution: launch/stop cell
  if(auto* exec = executionComponent())
  {
    // Determine quantization: per-cell launch mode, or fall back to global
    double rate = m_model.globalQuantization();
    switch(cell->launchMode())
    {
      case LaunchMode::Immediate:
        rate = 0.;
        break;
      case LaunchMode::QuantizedBeat:
        rate = 4.;
        break;
      case LaunchMode::QuantizedBar:
        rate = 1.;
        break;
      default:
        break; // use global quantization
    }

    // Toggle based on trigger style
    switch(cell->triggerStyle())
    {
      case TriggerStyle::Toggle:
        if(cell->cellState() == CellState::Playing
           || cell->cellState() == CellState::Queued)
          exec->stopCell(cell->id(), rate);
        else
          exec->launchCell(cell->id(), rate);
        break;

      case TriggerStyle::Trigger:
      case TriggerStyle::Retrigger:
      default:
        // Always launch (will stop existing if exclusive)
        exec->launchCell(cell->id(), rate);
        break;
    }
    return;
  }

  // During editing: select the cell itself
  m_context.context.selectionStack.pushNewSelection({cell});
}

void ClipLauncherPresenter::on_cellDoubleClicked(int lane, int scene)
{
  auto* cell = m_model.cellAt(lane, scene);
  if(!cell)
    return;

  // Navigate into the cell's interval for editing
  auto doc = score::IDocument::documentFromObject(m_model);
  if(!doc)
    return;

  auto base = score::IDocument::get<Scenario::ScenarioDocumentPresenter>(*doc);
  if(base)
    base->setDisplayedInterval(const_cast<Scenario::IntervalModel*>(&cell->interval()));
}

void ClipLauncherPresenter::on_sceneLaunchClicked(int scene)
{
  if(auto* exec = executionComponent())
  {
    exec->launchScene(scene);
  }
}

void ClipLauncherPresenter::on_dropOnCell(int lane, int scene, const QMimeData& mime)
{
  auto* cell = m_model.cellAt(lane, scene);
  if(!cell)
    return;

  // Use the existing DropProcessInInterval handler to add the process to the cell's interval
  Scenario::DropProcessInInterval handler;
  handler.drop(m_context.context, cell->interval(), QPointF{}, mime);
}

void ClipLauncherPresenter::fillContextMenu(
    QMenu& menu, QPoint pos, QPointF scenepos,
    const Process::LayerContextMenuManager& cm)
{
  auto viewPos = m_view->mapFromScene(scenepos);
  auto cellPos = m_view->cellAtPos(viewPos);

  if(cellPos)
  {
    int lane = cellPos->first;
    int scene = cellPos->second;
    auto* cell = m_model.cellAt(lane, scene);

    if(cell)
    {
      auto* removeAct = menu.addAction(tr("Remove cell"));
      connect(removeAct, &QAction::triggered, this, [this, cell] {
        CommandDispatcher<> disp{m_context.context.commandStack};
        disp.submit<RemoveCell>(m_model, *cell);
      });
    }
    else
    {
      auto* addAct = menu.addAction(tr("Add cell"));
      connect(addAct, &QAction::triggered, this, [this, lane, scene] {
        CommandDispatcher<> disp{m_context.context.commandStack};
        disp.submit<AddCell>(m_model, lane, scene);
      });
    }
  }

  menu.addSeparator();

  auto* addLaneAct = menu.addAction(tr("Add lane"));
  connect(addLaneAct, &QAction::triggered, this, [this] {
    CommandDispatcher<> disp{m_context.context.commandStack};
    disp.submit<AddLane>(m_model, m_model.laneCount());
  });

  auto* addSceneAct = menu.addAction(tr("Add scene"));
  connect(addSceneAct, &QAction::triggered, this, [this] {
    CommandDispatcher<> disp{m_context.context.commandStack};
    disp.submit<AddScene>(m_model, m_model.sceneCount());
  });
}

void ClipLauncherPresenter::on_laneHeaderClicked(int laneIdx)
{
  int idx = 0;
  for(auto& lane : m_model.lanes)
  {
    if(idx == laneIdx)
    {
      m_context.context.selectionStack.pushNewSelection({&lane});
      return;
    }
    idx++;
  }
}

void ClipLauncherPresenter::on_sceneHeaderClicked(int sceneIdx)
{
  int idx = 0;
  for(auto& scene : m_model.scenes)
  {
    if(idx == sceneIdx)
    {
      m_context.context.selectionStack.pushNewSelection({&scene});
      return;
    }
    idx++;
  }
}

void ClipLauncherPresenter::on_cellChanged(const CellModel&) { updateView(); }
void ClipLauncherPresenter::on_laneChanged(const LaneModel&) { updateView(); }
void ClipLauncherPresenter::on_sceneChanged(const SceneModel&) { updateView(); }

void ClipLauncherPresenter::updateView()
{
  QMetaObject::invokeMethod(this, [this] {
    if(m_view)
      m_view->update();
  }, Qt::QueuedConnection);
}

} // namespace ClipLauncher
