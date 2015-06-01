#include "SelectionToolState.hpp"
#include <QKeyEventTransition>
#include "Process/ScenarioGlobalCommandManager.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"


#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"

#include <iscore/document/DocumentInterface.hpp>

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/AbstractConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"

#include <QKeyEventTransition>
#include <QFinalState>
#include <QGraphicsScene>

SelectionToolState::SelectionToolState(ScenarioStateMachine& sm):
    ScenarioToolState{sm},
    m_dispatcher{iscore::IDocument::documentFromObject(m_sm.model())->selectionStack()}
{
    auto& scenario_view = m_sm.presenter().view();
    auto metaSelectionState = new QState{&m_localSM};
    m_localSM.setInitialState(metaSelectionState);
    metaSelectionState->setChildMode(QState::ChildMode::ParallelStates);
    metaSelectionState->setObjectName("metaSelectionState");
    {
        // Multi-selection state
        auto selectionModeState = new QState{metaSelectionState};
        selectionModeState->setObjectName("selectionModeState");
        {
            m_singleSelection = new QState{selectionModeState};

            selectionModeState->setInitialState(m_singleSelection);
            m_multiSelection = new QState{selectionModeState};

            auto trans1 = new QKeyEventTransition(&scenario_view,
                                                  QEvent::KeyPress, Qt::Key_Control, m_singleSelection);
            trans1->setTargetState(m_multiSelection);
            auto trans2 = new QKeyEventTransition(&scenario_view,
                                                  QEvent::KeyRelease, Qt::Key_Control, m_multiSelection);
            trans2->setTargetState(m_singleSelection);
        }


        /// Proper selection stuff
        auto selectionState = new QState{metaSelectionState};
        selectionState->setObjectName("selectionState");
        {
            // Wait
            m_waitState = new QState{selectionState};
            m_waitState->setObjectName("m_waitState");
            selectionState->setInitialState(m_waitState);

            // Area
            auto selectionAreaState = new QState{selectionState};
            selectionAreaState->setObjectName("selectionAreaState");

            make_transition<Press_Transition>(m_waitState, selectionAreaState);
            selectionAreaState->addTransition(selectionAreaState, SIGNAL(finished()), m_waitState);
            {
                // States
                auto pressAreaSelection = new QState{selectionAreaState};
                pressAreaSelection->setObjectName("pressAreaSelection");
                selectionAreaState->setInitialState(pressAreaSelection);
                auto moveAreaSelection = new QState{selectionAreaState};
                moveAreaSelection->setObjectName("moveAreaSelection");
                auto releaseAreaSelection = new QFinalState{selectionAreaState};
                releaseAreaSelection->setObjectName("releaseAreaSelection");

                // Transitions
                make_transition<Move_Transition>(pressAreaSelection, moveAreaSelection);
                make_transition<Release_Transition>(pressAreaSelection, releaseAreaSelection);

                make_transition<Move_Transition>(moveAreaSelection, moveAreaSelection);
                make_transition<Release_Transition>(moveAreaSelection, releaseAreaSelection);

                // Operations
                connect(pressAreaSelection, &QState::entered,
                        [&] () { m_initialPoint = m_sm.scenePoint; });
                connect(moveAreaSelection, &QState::entered,
                        [&] () {
                    m_movePoint = m_sm.scenePoint;
                    scenario_view.setSelectionArea(
                                QRectF{scenario_view.mapFromScene(m_initialPoint),
                                       scenario_view.mapFromScene(m_movePoint)}.normalized());
                    setSelectionArea(QRectF{m_initialPoint, m_movePoint}.normalized());
                });

                connect(releaseAreaSelection, &QState::entered,
                        [&] () { scenario_view.setSelectionArea(QRectF{}); });
            }

            // Deselection
            auto deselectState = new QState{selectionState};
            deselectState->setObjectName("deselectState");
            make_transition<Cancel_Transition>(selectionAreaState, deselectState);
            make_transition<Cancel_Transition>(m_waitState, deselectState);
            deselectState->addTransition(m_waitState);
            connect(deselectState, &QAbstractState::entered,
                    [&] () {
                m_dispatcher.setAndCommit(Selection{});
                scenario_view.setSelectionArea(QRectF{});
            });

            // Actions on selected elements
            auto t_delete = new QKeyEventTransition(
                        &scenario_view, QEvent::KeyPress, Qt::Key_Delete, m_waitState);
            connect(t_delete, &QAbstractTransition::triggered, [&] () {
                ScenarioGlobalCommandManager mgr{m_sm.commandStack()};
                mgr.deleteSelection(m_sm.model());
            });

            auto t_deleteContent = new QKeyEventTransition(
                        &scenario_view, QEvent::KeyPress, Qt::Key_Backspace, m_waitState);
            connect(t_deleteContent, &QAbstractTransition::triggered, [&] () {
                ScenarioGlobalCommandManager mgr{m_sm.commandStack()};
                mgr.clearContentFromSelection(m_sm.model());
            });

        }

    }
}


template<typename T>
Selection filterSelections(T* pressedModel,
                           Selection sel,
                           bool cumulation)
{
    if(!cumulation)
    {
        sel.clear();
    }

    // If the pressed element is selected
    if(pressedModel->selection.get())
    {
        if(cumulation)
        {
            sel.removeAll(pressedModel);
        }
        else
        {
            sel.push_back(pressedModel);
        }
    }
    else
    {
        sel.push_back(pressedModel);
    }

    return sel;
}



Selection filterSelections(const Selection& newSelection,
                           const Selection& currentSelection,
                           bool cumulation)
{
    return cumulation ? (newSelection + currentSelection).toSet().toList() : newSelection;
}



void SelectionToolState::on_scenarioPressed()
{
    using namespace std;
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const id_type<EventModel>& id) // Event
    {
        const auto& elts = m_sm.presenter().events();
        auto elt = find_if(begin(elts), end(elts),
                           [&] (EventPresenter* e) { return e->id() == id;});
        Q_ASSERT(elt != end(elts));

        m_dispatcher.setAndCommit(filterSelections(&(*elt)->model(),
                                                   m_sm.model().selectedChildren(),
                                                   m_multiSelection->active()));
    },
    [&] (const id_type<TimeNodeModel>& id) // TimeNode
    {
        const auto& elts = m_sm.presenter().timeNodes();
        auto elt = find_if(begin(elts), end(elts),
                           [&] (TimeNodePresenter* e) { return e->id() == id;});
        Q_ASSERT(elt != end(elts));

        m_dispatcher.setAndCommit(filterSelections(&(*elt)->model(),
                                                   m_sm.model().selectedChildren(),
                                                   m_multiSelection->active()));
    },
    [&] (const id_type<ConstraintModel>& id) // Constraint
    {
        const auto& elts = m_sm.presenter().constraints();
        auto elt = find_if(begin(elts), end(elts),
                           [&] (TemporalConstraintPresenter* e) { return e->id() == id;});
        Q_ASSERT(elt != end(elts));

        m_dispatcher.setAndCommit(filterSelections(&(*elt)->model(),
                                                   m_sm.model().selectedChildren(),
                                                   m_multiSelection->active()));
    },
    [&] () { m_localSM.postEvent(new Press_Event); });
}

void SelectionToolState::on_scenarioMoved()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const id_type<EventModel>&) { m_localSM.postEvent(new Move_Event); },
    [&] (const id_type<TimeNodeModel>&) { m_localSM.postEvent(new Move_Event); },
    [&] (const id_type<ConstraintModel>&) { m_localSM.postEvent(new Move_Event); },
    [&] () { m_localSM.postEvent(new Move_Event); });
}

void SelectionToolState::on_scenarioReleased()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const id_type<EventModel>&) { m_localSM.postEvent(new Release_Event); },
    [&] (const id_type<TimeNodeModel>&) { m_localSM.postEvent(new Release_Event); },
    [&] (const id_type<ConstraintModel>&) { m_localSM.postEvent(new Release_Event); },
    [&] () { m_localSM.postEvent(new Release_Event); });
}

void SelectionToolState::setSelectionArea(const QRectF& area)
{
    using namespace std;
    QPainterPath path;
    path.addRect(area);
    Selection sel;
    const auto& events = m_sm.presenter().events();
    const auto& timenodes = m_sm.presenter().timeNodes();
    const auto& cstrs = m_sm.presenter().constraints();

    const auto items = m_sm.scene().items(path);


    for (const auto& item : items)
    {
        auto ev_it = find_if(events.cbegin(),
                             events.cend(),
                             [&] (EventPresenter* p) { return p->view() == item; });
        if(ev_it != events.cend())
        {
            sel.push_back(&(*ev_it)->model());
        }
    }

    for (const auto& item : items)
    {
        auto tn_it = find_if(timenodes.cbegin(),
                             timenodes.cend(),
                             [&] (TimeNodePresenter* p) { return p->view() == item; });
        if(tn_it != timenodes.cend())
        {
            sel.push_back(&(*tn_it)->model());
        }
    }

    for (const auto& item : items)
    {
        auto cst_it = find_if(cstrs.begin(),
                              cstrs.end(),
                              [&] (TemporalConstraintPresenter* p) { return p->view() == item; });
        if(cst_it != cstrs.end())
        {
            sel.push_back(&(*cst_it)->model());
        }
    }

    m_dispatcher.setAndCommit(filterSelections(sel,
                                               m_sm.model().selectedChildren(),
                                               m_multiSelection->active()));
}


