#include "MoveDeckToolState.hpp"

MoveDeckToolState::MoveDeckToolState(ScenarioStateMachine& sm):
    GenericToolState{sm}
{
    // TODO faire DeckRemoveState, BoxMove, BoxRemoveState ?

    m_waitState = new QState;
    m_localSM.addState(m_waitState);
    m_localSM.setInitialState(m_waitState);


}

void MoveDeckToolState::on_scenarioPressed()
{

}

void MoveDeckToolState::on_scenarioMoved()
{

}

void MoveDeckToolState::on_scenarioReleased()
{

}

