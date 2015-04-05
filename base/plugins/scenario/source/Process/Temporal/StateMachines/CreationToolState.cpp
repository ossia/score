#include "CreationToolState.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioViewModel.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/Event/EventPresenter.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"

#include "Process/ScenarioGlobalCommandManager.hpp"
#include "ScenarioStateMachine.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <iscore/command/OngoingCommandManager.hpp>

#include <algorithm>
#include <QRectF>
#include <QGraphicsItem>
#include <QGraphicsScene>


using namespace iscore;
CreationToolState::CreationToolState(ScenarioStateMachine& sm) :
    m_sm{sm}
{
    // The base states of the tool
    QState* creationState_wait = new QState;
    m_createEvent = new CreateEventState{
                    iscore::IDocument::path(m_sm.model()),
                    m_sm.commandStack(), nullptr};

    m_createEvent->addTransition(m_createEvent, SIGNAL(finished()), creationState_wait);

    auto t_click = new ScenarioPress_Transition;
    t_click->setTargetState(this); // Check the tool
    connect(t_click, &QAbstractTransition::triggered,
            [&] () {
        on_scenarioPressed();
    });
    this->addTransition(t_click);
    auto t_move = new ScenarioMove_Transition;
    t_move->setTargetState(this); // Check the tool
    connect(t_move, &QAbstractTransition::triggered,
            [&] () {
        on_scenarioMoved();
    });
    this->addTransition(t_move);

    auto t_rel = new ScenarioRelease_Transition;
    t_rel->setTargetState(this); // Check the tool
    connect(t_rel, &QAbstractTransition::triggered,
            [&] () {
        on_scenarioReleased();
    });
    this->addTransition(t_rel);

    auto t1 = new ClickOnEvent_Transition(*m_createEvent);
    t1->setTargetState(m_createEvent);
    creationState_wait->addTransition(t1);

    auto t2 = new ClickOnNothing_Transition(*m_createEvent);
    t2->setTargetState(m_createEvent);
    creationState_wait->addTransition(t2);

    // TODO ClickOnTimeNode_Transition


    ct_sm.addState(creationState_wait);
    ct_sm.addState(m_createEvent);

    ct_sm.setInitialState(creationState_wait);
    ct_sm.start();

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
void CreationToolState::mapItemToAction(
        QGraphicsItem* pressedItem,
        EventFun&& ev_fun,
        TimeNodeFun&& tn_fun,
        ConstraintFun&& cst_fun,
        NothingFun&& nothing_fun)
{
    if(auto ev = dynamic_cast<EventView*>(pressedItem))
    {
        ev_fun(getPresenterFromView(ev, m_sm.presenter().events())->model()->id());
    }
    else if(auto tn = dynamic_cast<TimeNodeView*>(pressedItem))
    {
        tn_fun(getPresenterFromView(tn, m_sm.presenter().timeNodes())->model()->id());
    }
    else if(auto cst = dynamic_cast<AbstractConstraintView*>(pressedItem))
    { // TODO Unnecessary
        cst_fun(getPresenterFromView(cst, m_sm.presenter().constraints())->abstractConstraintViewModel()->model()->id());
    }
    else
    {
        nothing_fun();
    }
}

auto CreationToolState::itemUnderMouse(const QPointF& point)
{
    return m_sm.presenter().view().scene()->itemAt(point, QTransform());
}

void CreationToolState::on_scenarioPressed()
{
    mapItemToAction(itemUnderMouse(m_sm.scenePoint),
    [&] (const auto& id)
    { ct_sm.postEvent(new ClickOnEvent_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { ct_sm.postEvent(new ClickOnTimeNode_Event{id, m_sm.scenarioPoint}); },
    [&] (const auto& id)
    { ct_sm.postEvent(new ClickOnConstraint_Event{id, m_sm.scenarioPoint}); }, // TODO Unnecessary
    [&] ()
    { ct_sm.postEvent(new ClickOnNothing_Event{m_sm.scenarioPoint}); });
}


void CreationToolState::on_scenarioMoved()
{
    auto collidingEvents = getCollidingModels(m_sm.presenter().events(),
                                              m_createEvent->createdEvent());
    if(!collidingEvents.empty())
    {
        auto model = collidingEvents.first();

        ct_sm.postEvent(new MoveOnEvent_Event{
                           model->id(), m_sm.scenarioPoint});

        return;
    }

    auto collidingTimeNodes = getCollidingModels(m_sm.presenter().timeNodes(),
                                                 m_createEvent->createdTimeNode());
    if(!collidingTimeNodes.empty())
    {
        auto model = collidingTimeNodes.first();

        ct_sm.postEvent(new MoveOnTimeNode_Event{
                           model->id(), m_sm.scenarioPoint});

        return;
    }

    ct_sm.postEvent(new MoveOnNothing_Event{m_sm.scenarioPoint});
}

void CreationToolState::on_scenarioReleased()
{
    auto collidingEvents = getCollidingModels(m_sm.presenter().events(),
                                              m_createEvent->createdEvent());
    if(!collidingEvents.empty())
    {
        auto model = collidingEvents.first();
        ct_sm.postEvent(new ReleaseOnEvent_Event{model->id(), m_sm.scenarioPoint});

        return;
    }

    auto collidingTimeNodes = getCollidingModels(m_sm.presenter().timeNodes(),
                                                 m_createEvent->createdTimeNode());
    if(!collidingTimeNodes.empty())
    {
        auto model = collidingTimeNodes.first();
        ct_sm.postEvent(new ReleaseOnTimeNode_Event{model->id(), m_sm.scenarioPoint});

        return;
    }
    ct_sm.postEvent(new ReleaseOnNothing_Event{m_sm.scenarioPoint});
}
