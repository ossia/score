#include <Loop/LoopLayer.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Loop/LoopView.hpp>
#include <QQuickPaintedItem>
#include <iscore/widgets/GraphicsItem.hpp>
#include <tuple>
#include <type_traits>

#include "Loop/LoopViewUpdater.hpp"
#include "LoopPresenter.hpp"
#include <ossia/detail/algorithms.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/tools/Todo.hpp>

#include <QMenu>
#include <Scenario/Application/Menus/ObjectMenuActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/ConstraintActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/EventActions.hpp>
#include <Scenario/Application/Menus/ObjectsActions/StateActions.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Process/ScenarioGlobalCommandManager.hpp>
#include <iscore/application/ApplicationContext.hpp>

namespace Process
{
class LayerModel;
}
namespace Process
{
class ProcessModel;
}
class QMenu;
class QObject;
namespace Scenario
{
struct VerticalExtent;
}
namespace Loop
{
LayerPresenter::LayerPresenter(
    const Layer& layer,
    LayerView* view,
    const Process::ProcessPresenterContext& ctx,
    QObject* parent)
    : Process::LayerPresenter{ctx, parent}
    , BaseScenarioPresenter<Loop::ProcessModel, Scenario::TemporalConstraintPresenter>{layer
                                                                                           .model()}
    , m_layer{layer}
    , m_view{view}
    , m_viewUpdater{*this}
    , m_palette{m_layer.model(), *this, m_context, *m_view}
{
  using namespace Scenario;
  m_constraintPresenter
      = new TemporalConstraintPresenter{layer.constraint(), ctx, view, this};
  m_startStatePresenter = new StatePresenter{
      layer.model().BaseScenarioContainer::startState(), m_view, this};
  m_endStatePresenter = new StatePresenter{
      layer.model().BaseScenarioContainer::endState(), m_view, this};
  m_startEventPresenter
      = new EventPresenter{layer.model().startEvent(), m_view, this};
  m_endEventPresenter
      = new EventPresenter{layer.model().endEvent(), m_view, this};
  m_startNodePresenter
      = new TimeNodePresenter{layer.model().startTimeNode(), m_view, this};
  m_endNodePresenter
      = new TimeNodePresenter{layer.model().endTimeNode(), m_view, this};

  auto elements = std::make_tuple(
      m_constraintPresenter,
      m_startStatePresenter,
      m_endStatePresenter,
      m_startEventPresenter,
      m_endEventPresenter,
      m_startNodePresenter,
      m_endNodePresenter);

  ossia::for_each_in_tuple(elements, [&](auto elt) {
    using elt_t = std::remove_reference_t<decltype(*elt)>;
    connect(elt, &elt_t::pressed, this, &LayerPresenter::pressed);
    connect(elt, &elt_t::moved, this, &LayerPresenter::moved);
    connect(elt, &elt_t::released, this, &LayerPresenter::released);
  });

  con(m_endEventPresenter->model(), &EventModel::extentChanged, this,
      [=](const VerticalExtent&) {
        m_viewUpdater.updateEvent(*m_endEventPresenter);
      });
  con(m_endEventPresenter->model(), &EventModel::dateChanged, this,
      [=](const TimeVal&) {
        m_viewUpdater.updateEvent(*m_endEventPresenter);
      });
  con(m_endNodePresenter->model(), &TimeNodeModel::extentChanged, this,
      [=](const VerticalExtent&) {
        m_viewUpdater.updateTimeNode(*m_endNodePresenter);
      });
  con(m_endNodePresenter->model(), &TimeNodeModel::dateChanged, this,
      [=](const TimeVal&) {
        m_viewUpdater.updateTimeNode(*m_endNodePresenter);
      });

  connect(
      m_view, &LayerView::askContextMenu, this,
      &LayerPresenter::contextMenuRequested);
  connect(m_view, &LayerView::pressed, this, [&]() {
    m_context.context.focusDispatcher.focus(this);
  });

  con(ctx.updateTimer, &QTimer::timeout, this,
      &LayerPresenter::on_constraintExecutionTimer);

  m_constraintPresenter->view()->unsetCursor();
}

LayerPresenter::~LayerPresenter()
{
  disconnect(
      &m_context.context.updateTimer, &QTimer::timeout, this,
      &LayerPresenter::on_constraintExecutionTimer);

  delete m_constraintPresenter;
  delete m_startStatePresenter;
  delete m_endStatePresenter;
  delete m_startEventPresenter;
  delete m_endEventPresenter;
  delete m_startNodePresenter;
  delete m_endNodePresenter;
}

void LayerPresenter::on_constraintExecutionTimer()
{
  m_constraintPresenter->on_playPercentageChanged(
      m_constraintPresenter->model().duration.playPercentage());
}

void LayerPresenter::setWidth(qreal width)
{
  m_view->setWidth(width);
}

void LayerPresenter::setHeight(qreal height)
{
  m_view->setHeight(height);
}

void LayerPresenter::putToFront()
{
  //m_view->setFlag(QQuickPaintedItem::ItemStacksBehindParent, false);
  m_view->setOpacity(1);
}

void LayerPresenter::putBehind()
{
  //m_view->setFlag(QQuickPaintedItem::ItemStacksBehindParent, true);
  m_view->setOpacity(0.1);
}

void LayerPresenter::on_zoomRatioChanged(ZoomRatio val)
{
  m_zoomRatio = val;
  m_constraintPresenter->on_zoomRatioChanged(m_zoomRatio);
}

void LayerPresenter::parentGeometryChanged()
{
  updateAllElements();
  m_view->update();
}

const Process::LayerModel& LayerPresenter::layerModel() const
{
  return m_layer;
}

const Id<Process::ProcessModel>& LayerPresenter::modelId() const
{
  return m_layer.model().id();
}

void LayerPresenter::updateAllElements()
{
  m_viewUpdater.updateConstraint(*m_constraintPresenter);
  m_viewUpdater.updateEvent(*m_startEventPresenter);
  m_viewUpdater.updateEvent(*m_endEventPresenter);
  m_viewUpdater.updateTimeNode(*m_startNodePresenter);
  m_viewUpdater.updateTimeNode(*m_endNodePresenter);
}

void LayerPresenter::fillContextMenu(
    QMenu& menu,
    QPoint pos,
    QPointF scenepos,
    const Process::LayerContextMenuManager&) const
{
  // TODO ACTIONS
  /*
  auto selected = layerModel().processModel().selectedChildren();

  auto& appPlug =
  m_context.context.app.applicationPlugin<Scenario::ScenarioApplicationPlugin>();
  for(auto elt : appPlug.pluginActions())
  {
      if(auto oma = dynamic_cast<Scenario::ObjectMenuActions*>(elt))
      {
          oma->eventActions()->fillContextMenu(menu, selected, pos, scenepos);
          if(m_model.constraint().selection.get())
              oma->constraintActions()->fillContextMenu(menu, selected,
  m_layer.constraint(), pos, scenepos);
          menu->addSeparator();
      }
  }
  */
}

void clearContentFromSelection(
    const ProcessModel& model, const iscore::CommandStackFacade& stack)
{
  clearContentFromSelection(
      static_cast<const Scenario::BaseScenarioContainer&>(model), stack);
}
}
