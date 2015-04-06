#include "SelectionToolState.hpp"
#include <QKeyEventTransition>

SelectionToolState::SelectionToolState(ScenarioStateMachine& sm):
    GenericToolState{sm}
{
    // Here we use parallel states.
    // Meta-state 1
    auto selectionModeState = new QState;
    auto singleSelection = new QState{selectionModeState};
    auto multiSelection = new QState{selectionModeState};
    m_localSM.addState(selectionModeState);

    auto trans1 = new QKeyEventTransition(singleSelection, QEvent::KeyPress, Qt::Key_Control, multiSelection);
    auto trans2 = new QKeyEventTransition(multiSelection, QEvent::KeyRelease, Qt::Key_Control, singleSelection);

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

}

void SelectionToolState::on_scenarioMoved()
{

}

void SelectionToolState::on_scenarioReleased()
{

}

