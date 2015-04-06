#include "SelectionToolState.hpp"
#include <QKeyEventTransition>
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"

#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include <QGraphicsScene>

SelectionToolState::SelectionToolState(ScenarioStateMachine& sm):
    GenericToolState{sm},
    m_dispatcher{iscore::IDocument::documentFromObject(m_sm.model())->selectionStack()}
{
    // Multi-selection state
    auto selectionModeState = new QState;
    m_singleSelection = new QState{selectionModeState};
    m_multiSelection = new QState{selectionModeState};
    m_localSM.addState(selectionModeState);

    auto trans1 = new QKeyEventTransition(&m_sm.presenter().view(),
                                          QEvent::KeyPress, Qt::Key_Control, m_singleSelection);
    trans1->setTargetState(m_multiSelection);
    auto trans2 = new QKeyEventTransition(&m_sm.presenter().view(),
                                          QEvent::KeyRelease, Qt::Key_Control, m_multiSelection);
    trans2->setTargetState(m_singleSelection);


    /// Area
    auto selectionAreaState = new QState;
    m_localSM.addState(selectionAreaState);

    auto t_wait_press = new ScenarioPress_Transition;
    t_wait_press->setTargetState(selectionAreaState);
    m_waitState->addTransition(t_wait_press);


    // States
    auto pressAreaSelection = new QState{selectionAreaState};
    selectionAreaState->setInitialState(pressAreaSelection);
    auto moveAreaSelection = new QState{selectionAreaState};
    auto releaseAreaSelection = new QState{selectionAreaState};

    // Transitions
    auto t_press_move = new ScenarioMove_Transition;
    t_press_move->setTargetState(moveAreaSelection);
    pressAreaSelection->addTransition(t_press_move);

    auto t_move_move = new ScenarioMove_Transition;
    t_move_move->setTargetState(moveAreaSelection);
    moveAreaSelection->addTransition(t_move_move);

    auto t_move_release = new ScenarioRelease_Transition;
    t_move_release->setTargetState(releaseAreaSelection);
    moveAreaSelection->addTransition(t_move_release);

    releaseAreaSelection->addTransition(m_waitState);

    // Operations
    connect(pressAreaSelection, &QState::entered,
            [&] () {
        m_initialPoint = m_sm.scenePoint;
    });
    connect(moveAreaSelection, &QState::entered,
            [&] () {
        m_movePoint = m_sm.scenePoint;
        // TODO careful if m_movepoint is not top left ?
        m_sm.presenter().view().setSelectionArea({m_sm.presenter().view().mapFromScene(m_initialPoint),
                                                  m_sm.presenter().view().mapFromScene(m_movePoint)});
        setSelectionArea({m_initialPoint, m_movePoint});
    });

    connect(releaseAreaSelection, &QState::entered,
            [&] () {
        m_sm.presenter().view().setSelectionArea(QRectF{});
        // TODO cleanup ?
    });


    // TODO rollback
}


template<typename T>
Selection filterSelections(T* pressedModel, Selection sel, bool cumulation)
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



void SelectionToolState::on_scenarioPressed()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const auto& id) // Event
    {
        using namespace std;
        const auto& elts = m_sm.presenter().events();
        auto elt = find_if(begin(elts),
                           end(elts),
                           [&] (EventPresenter* e) { return e->id() == id;});
        Q_ASSERT(elt != end(elts));

        m_dispatcher.setAndCommit(filterSelections((*elt)->model(),
                                                   m_sm.model().selectedChildren(),
                                                   m_multiSelection->active()));
    },
    [&] (const auto& id) // TimeNode
    {
        using namespace std;
        const auto& elts = m_sm.presenter().timeNodes();
        auto elt = find_if(begin(elts),
                           end(elts),
                           [&] (TimeNodePresenter* e) { return e->id() == id;});
        Q_ASSERT(elt != end(elts));

        m_dispatcher.setAndCommit(filterSelections((*elt)->model(),
                                                   m_sm.model().selectedChildren(),
                                                   m_multiSelection->active()));
    },
    [&] (const auto& id) // Constraint
    {
        using namespace std;
        const auto& elts = m_sm.presenter().constraints();
        auto elt = find_if(begin(elts),
                           end(elts),
                           [&] (TemporalConstraintPresenter* e) { return e->id() == id;});
        Q_ASSERT(elt != end(elts));

        m_dispatcher.setAndCommit(filterSelections((*elt)->model(),
                                                   m_sm.model().selectedChildren(),
                                                   m_multiSelection->active()));
    },
    [&] () { m_localSM.postEvent(new ScenarioPress_Event); });
}

void SelectionToolState::on_scenarioMoved()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const auto&) {},
    [&] (const auto&) {},
    [&] (const auto&) {},
    [&] () { m_localSM.postEvent(new ScenarioMove_Event); });
}

void SelectionToolState::on_scenarioReleased()
{
    mapTopItem(itemUnderMouse(m_sm.scenePoint),
    [&] (const auto&) {},
    [&] (const auto&) {},
    [&] (const auto&) {},
    [&] () { m_localSM.postEvent(new ScenarioRelease_Event); });
}

void SelectionToolState::setSelectionArea(const QRectF& area)
{
    QPainterPath path;
    path.addRect(area);
    Selection sel;

    for (auto item : m_sm.presenter().view().scene()->items(path))
    {
        EventView* itemEv = dynamic_cast<EventView*>(item);
        auto itemCstr = dynamic_cast<TemporalConstraintView*>(item);
        auto itemTn = dynamic_cast<TimeNodeView*>(item);

        if (itemEv)
        {
            for (EventPresenter* event : m_sm.presenter().events())
            {
                if (event->view() == itemEv)
                {
                    sel.push_back(event->model());
                    break;
                }
            }
        }
        else if (itemCstr)
        {
            for (TemporalConstraintPresenter* cstr : m_sm.presenter().constraints())
            {
                if (view(cstr) == itemCstr)
                {
                    sel.push_back(viewModel(cstr)->model());
                    break;
                }
            }
        }

        else if (itemTn)
        {
            for (TimeNodePresenter* tn : m_sm.presenter().timeNodes())
            {
                if (tn->view() == item)
                {
                    sel.push_back(tn->model());
                    break;
                }
            }
        }
    }

    // TODO if m_multiSelection->active()
    m_dispatcher.setAndCommit(sel);
    // TODO focus();
}


