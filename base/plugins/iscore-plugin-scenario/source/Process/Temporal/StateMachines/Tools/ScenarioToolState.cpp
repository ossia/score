#include "ScenarioToolState.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"
#include <QGraphicsScene>

QGraphicsItem *ScenarioToolState::itemUnderMouse(const QPointF &point) const
{
    return m_scene.itemAt(point, QTransform());
}

ScenarioToolState::ScenarioToolState(const ScenarioStateMachine &sm) :
    m_sm{sm},
    m_scene{m_sm.scene()}
{
    auto t_click = make_transition<Press_Transition>(this, this);
    connect(t_click, &QAbstractTransition::triggered,
            this,    &ScenarioToolState::on_scenarioPressed);

    auto t_move = make_transition<Move_Transition>(this, this);
    connect(t_move, &QAbstractTransition::triggered,
            this,   &ScenarioToolState::on_scenarioMoved);

    auto t_rel = make_transition<Release_Transition>(this, this);
    connect(t_rel, &QAbstractTransition::triggered,
            this,  &ScenarioToolState::on_scenarioReleased);

    auto t_cancel = make_transition<Cancel_Transition>(this, this);
    connect(t_cancel, &QAbstractTransition::triggered,
            [&] () { m_localSM.postEvent(new Cancel_Event); });
}

void ScenarioToolState::start()
{
    m_localSM.start();
}

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
const id_type<EventModel> &ScenarioToolState::itemToEventId(const QGraphicsItem * pressedItem) const
{
    return static_cast<const EventView*>(pressedItem)->presenter().model().id();
}

const id_type<TimeNodeModel> &ScenarioToolState::itemToTimeNodeId(const QGraphicsItem *pressedItem) const
{
    return static_cast<const TimeNodeView*>(pressedItem)->presenter().model().id();
}

const id_type<ConstraintModel> &ScenarioToolState::itemToConstraintId(const QGraphicsItem *pressedItem) const
{
    return static_cast<const AbstractConstraintView*>(pressedItem)->presenter().abstractConstraintViewModel().model().id();
}
