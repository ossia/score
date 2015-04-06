#include "SelectionToolState.hpp"

SelectionToolState::SelectionToolState(ScenarioStateMachine& sm):
    GenericToolState{sm}
{
    // Here we use parallel states.
    // Meta-state 1
    auto selectionModeState = new QState;
    auto singleSelection = new QState{selectionModeState};
    auto multiSelection = new QState{selectionModeState};

    // Meta-state 2
    auto selectionControlState = new QState;
    auto pressAreaSelection = new QState{selectionControlState};
    auto moveAreaSelection = new QState{selectionControlState};
    auto releaseAreaSelection = new QState{selectionControlState};
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

