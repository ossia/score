#include "ScenarioCommandManager.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "source/Process/Temporal/TemporalScenarioViewModel.hpp"
#include "source/Process/Temporal/TemporalScenarioView.hpp"
#include "TemporalScenarioPresenter.hpp"

#include <core/document/Document.hpp>
#include <core/document/DocumentPresenter.hpp>

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventData.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ConstraintData.hpp"

#include "Commands/Scenario/Creations/CreateEvent.hpp"
#include "Commands/Scenario/Creations/CreateEventAfterEvent.hpp"
#include "Commands/Scenario/Creations/CreateEventAfterEventOnTimeNode.hpp"
#include "Commands/Scenario/Creations/CreateConstraint.hpp"
#include "Commands/Scenario/Displacement/MoveEvent.hpp"
#include "Commands/Scenario/Displacement/MoveTimeNode.hpp"
#include "Commands/Scenario/Displacement/MoveConstraint.hpp"
#include "Commands/TimeNode/MergeTimeNodes.hpp"
#include "Process/ScenarioGlobalCommandManager.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <iscore/command/OngoingCommandManager.hpp>
#include <algorithm>
#include <QRectF>
#include <QGraphicsItem>
#include <QGraphicsScene>

using namespace Scenario::Command;

using namespace iscore;
ScenarioCommandManager::ScenarioCommandManager(TemporalScenarioPresenter& presenter) :
    QObject{&presenter},
    m_presenter{presenter},
    m_commandStack{iscore::IDocument::documentFromObject(m_presenter.m_viewModel->sharedProcessModel())->commandStack()},
    m_locker{iscore::IDocument::documentFromObject(m_presenter.m_viewModel->sharedProcessModel())->locker()},
    m_creationCommandDispatcher{new LockingOngoingCommandDispatcher<MergeStrategy::Simple>{
                                m_presenter.m_viewModel->sharedProcessModel(),
                                m_locker,
                                m_commandStack,
                                this}},
    m_moveCommandDispatcher{new LockingOngoingCommandDispatcher<MergeStrategy::Simple, CommitStrategy::Redo>{
                            m_presenter.m_viewModel->sharedProcessModel(),
                            m_locker,
                            m_commandStack,
                            this}},
    m_instantCommandDispatcher{new CommandDispatcher<SendStrategy::Simple>{
                               m_commandStack,
                               this}}
{

    // TODO make it more generic (maybe with a QAction ?)
    connect(m_presenter.m_view, &TemporalScenarioView::deletePressed,
            this, [&] ()
    {
        ScenarioGlobalCommandManager mgr{m_commandStack};
        mgr.deleteSelection(model(*m_presenter.m_viewModel));
    });

    connect(m_presenter.m_view, &TemporalScenarioView::clearPressed,
            this, [&] ()
    {
        ScenarioGlobalCommandManager mgr{m_commandStack};
        mgr.clearContentFromSelection(model(*m_presenter.m_viewModel));
    });

    connect(m_presenter.m_view, &TemporalScenarioView::scenarioPressed,
            this, &ScenarioCommandManager::on_scenarioPressed);
    connect(m_presenter.m_view, &TemporalScenarioView::scenarioReleased,
            this, &ScenarioCommandManager::on_scenarioReleased);


    ////// Try to make a state machine with states :
    /// Selection
    /// Constraint creation between two events
    /// Constraint + event creation
    /// Constraint + event + timenode creation from nowhere
    /// Event adding in timenode
    /// Event move
    /// Constraint move
    /// Timenode move
    ///

    // The ScenarioControl should register the StateMachine for the toolbar.
    // It should then propagate signals to the focused scenario.
    //QState* selectionState = new QState;
    QState* creationState = new QState;
    QState* creationState_wait = new QState{creationState};
    creationState->setInitialState(creationState_wait);
    m_createEvent = new CreateEventState{iscore::IDocument::path(m_presenter.m_viewModel->sharedProcessModel()), m_commandStack, creationState};
    //QState* moveState = new QState;

    //m_sm.addState(selectionState);
    m_sm.addState(creationState);
    //m_sm.addState(moveState);

    // TODO utiiser events et postEvent (avec les id des timenode, event, sous lesquelles on se trouve)
    // pour faire évoluer la machine correctement
    // peu importe l'outil dans lequel on se trouve

    m_sm.setInitialState(creationState);
    auto t1 = new ClickOnEvent_Transition(*m_createEvent);
    t1->setTargetState(m_createEvent);
    creationState_wait->addTransition(t1);

    auto t2 = new ClickOnNothing_Transition(*m_createEvent);
    t2->setTargetState(m_createEvent);
    creationState_wait->addTransition(t2);
    //creationState_wait->addTransition(m_presenter.m_view, SIGNAL(scenarioPressed(QPointF)), m_createEvent);
    m_createEvent->addTransition(m_createEvent, SIGNAL(finished()), creationState_wait);

    m_sm.start();
    // TODO The machine should be started when the process gets the focus ?

}

template<typename PresenterArray, typename IdToIgnore>
auto getCollidingModels(const PresenterArray& array, const IdToIgnore& id)
{
    using namespace std;

    QList<decltype(array[0]->model())> colliding;
    for(const auto& elt : array)
    {
        if((!bool(id) || id != elt->id()) && elt->view()->isUnderMouse())
        {
            colliding.push_back(elt->model());
        }
    }
    // TODO sort the elements according to their Z pos.

    return colliding;
}


template<typename EventFun,
         typename TimeNodeFun,
         typename ConstraintFun,
         typename NothingFun>
void ScenarioCommandManager::mapItemToAction(
        QGraphicsItem* pressedItem,
        EventFun&& ev_fun,
        TimeNodeFun&& tn_fun,
        ConstraintFun&& cst_fun,
        NothingFun&& nothing_fun)
{
    if(auto ev = dynamic_cast<EventView*>(pressedItem))
    {
        ev_fun(getPresenterFromView(ev, m_presenter.m_events)->model()->id());
    }
    else if(auto tn = dynamic_cast<TimeNodeView*>(pressedItem))
    {
        tn_fun(getPresenterFromView(tn, m_presenter.m_timeNodes)->model()->id());
    }
    else if(auto cst = dynamic_cast<AbstractConstraintView*>(pressedItem))
    {
        cst_fun(getPresenterFromView(cst, m_presenter.m_constraints)->abstractConstraintViewModel()->model()->id());
    }
    else
    {
        nothing_fun();
    }
}

auto ScenarioCommandManager::itemUnderMouse(const QPointF& point)
{
    return m_presenter.m_view->scene()->itemAt(m_presenter.m_view->mapToScene(point), QTransform());
}

void ScenarioCommandManager::on_scenarioPressed(const QPointF& point)
{
    auto date = TimeValue::fromMsecs(point.x() * m_presenter.m_zoomRatio);
    auto y = point.y() /  m_presenter.m_view->boundingRect().height();

    mapItemToAction(itemUnderMouse(point),
    [&] (const auto& id)
    {
        m_sm.postEvent(new ClickOnEvent_Event{
                           id, date, y});
    },
    [&] (const auto& id)
    {
        m_sm.postEvent(new ClickOnTimeNode_Event{
                        id, date, y});
    },
    [&] (const auto& id)
    {
        m_sm.postEvent(new ClickOnConstraint_Event{
                           id, date, y});
    },
    [&] ()
    {
        m_sm.postEvent(new ClickOnNothing_Event{date, y});
    });
}


void ScenarioCommandManager::on_scenarioMoved(const QPointF& point)
{
    auto date = TimeValue::fromMsecs(point.x() * m_presenter.m_zoomRatio);
    auto y    = point.y() /  m_presenter.m_view->boundingRect().height();
    auto collidingEvents = getCollidingModels(m_presenter.m_events, m_createEvent->createdEvent());
    if(!collidingEvents.empty())
    {
        auto model = collidingEvents.first();
        m_sm.postEvent(new MoveOnEvent_Event{
                           model->id(), date, y});

        return;
    }

    auto collidingTimeNodes = getCollidingModels(m_presenter.m_timeNodes, m_createEvent->createdTimeNode());
    if(!collidingTimeNodes.empty())
    {
        auto model = collidingTimeNodes.first();
        m_sm.postEvent(new MoveOnTimeNode_Event{
                           model->id(), date, y});

        return;
    }

    m_sm.postEvent(new MoveOnNothing_Event{date, y});
}

void ScenarioCommandManager::on_scenarioReleased(const QPointF& point)
{
    m_sm.postEvent(new ReleaseOnNothing_Event{
                       TimeValue::fromMsecs(point.x() * m_presenter.m_zoomRatio),
                       point.y() /  m_presenter.m_view->boundingRect().height()});

}
