#include "SelectionToolState.hpp"
#include <QKeyEventTransition>

SelectionToolState::SelectionToolState(ScenarioStateMachine& sm):
    GenericToolState{sm}
{
    // Here we use parallel states.
    // Meta-state 1
    auto selectionModeState = new QState;
    m_singleSelection = new QState{selectionModeState};
    m_multiSelection = new QState{selectionModeState};
    m_localSM.addState(selectionModeState);

    auto trans1 = new QKeyEventTransition(m_singleSelection, QEvent::KeyPress, Qt::Key_Control, m_multiSelection);
    auto trans2 = new QKeyEventTransition(m_multiSelection, QEvent::KeyRelease, Qt::Key_Control, m_singleSelection);

    sm.presenter().view().scene();
    // Meta-state 2
    auto selectionControlState = new QState;
    auto pressAreaSelection = new QState{selectionControlState};
    auto moveAreaSelection = new QState{selectionControlState};
    auto releaseAreaSelection = new QState{selectionControlState};
    auto waitState = new QState{selectionControlState};
    m_localSM.addState(selectionControlState);
}

void SelectionToolState::on_scenarioPressed()
{
    if(m_multiSelection->active())
    {
        mapTopItem(itemUnderMouse(m_sm.scenePoint),
        [&] (const auto& id) // Event
        {  },
        [&] (const auto& id) // Constraint
        {  },
        [&] (const auto& id) // TimeNode
        {  },
        [&] () { }); // TODO last case should not need to be here.
    }
    else
    {
        mapTopItem(itemUnderMouse(m_sm.scenePoint),
        [&] (const auto& id)
        {  },
        [&] (const auto& id)
        {  },
        [&] (const auto& id)
        {  },
        [&] () { }); // TODO last case should not need to be here.
    }
}

void SelectionToolState::on_scenarioMoved()
{

}

void SelectionToolState::on_scenarioReleased()
{

}

