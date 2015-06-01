#include "DeckTransitions.hpp"
ClickOnDeckOverlay_Transition::ClickOnDeckOverlay_Transition(DeckState &state):
    m_state{state}
{

}

DeckState &ClickOnDeckOverlay_Transition::state() const
{
    return m_state;
}

void ClickOnDeckOverlay_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<event_type*>(ev);

    this->state().currentDeck = std::move(qev->path);
}


ClickOnDeckHandle_Transition::ClickOnDeckHandle_Transition(DeckState &state):
    m_state{state}
{

}


DeckState &ClickOnDeckHandle_Transition::state() const
{
    return m_state;
}


void ClickOnDeckHandle_Transition::onTransition(QEvent *ev)
{
    auto qev = static_cast<event_type*>(ev);

    this->state().currentDeck = std::move(qev->path);
}
