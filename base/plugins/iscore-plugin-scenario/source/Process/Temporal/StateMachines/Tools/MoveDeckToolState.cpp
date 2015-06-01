#include "MoveDeckToolState.hpp"
#include "States/DeckStates.hpp"

#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/Deck/DeckPresenter.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/Box/Deck/DeckOverlay.hpp"
#include "Document/Constraint/Box/Deck/DeckView.hpp"
#include "Document/Constraint/Box/Deck/DeckHandle.hpp"

#include "Process/Temporal/StateMachines/Transitions/DeckTransitions.hpp"
#include <QFinalState>

MoveDeckToolState::MoveDeckToolState(const ScenarioStateMachine& sm):
    GenericStateBase{sm},
    m_dispatcher{m_sm.commandStack()},
    m_ongoingDispatcher{m_sm.commandStack()}
{
    /// 1. Set the scenario in the correct state with regards to this tool.
    connect(this, &QState::entered,
            [&] ()
    {
        for(TemporalConstraintPresenter* constraint : m_sm.presenter().constraints())
        {
            if(!constraint->box()) continue;
            constraint->box()->setDisabledDeckState();
        }
    });
    connect(this, &QState::exited,
            [&] ()
    {
        for(TemporalConstraintPresenter* constraint : m_sm.presenter().constraints())
        {
            if(!constraint->box()) continue;
            constraint->box()->setEnabledDeckState();
        }
    });

    /// 2. Setup the sub-state machine.
    m_waitState = new QState{&m_localSM};
    m_localSM.setInitialState(m_waitState);
    // Two states : one for moving the content of the deck, one for resizing with the handle.
    {
        auto dragDeck = new DragDeckState{m_dispatcher, m_sm, *m_sm.presenter().view().scene(), &m_localSM};
        // Enter the state
        make_transition<ClickOnDeckOverlay_Transition>(
                    m_waitState,
                    dragDeck,
                    *dragDeck);

        dragDeck->addTransition(dragDeck, SIGNAL(finished()), m_waitState);
    }

    {
        auto resizeDeck = new ResizeDeckState{m_ongoingDispatcher, m_sm, &m_localSM};
        make_transition<ClickOnDeckHandle_Transition>(
                    m_waitState,
                    resizeDeck,
                    *resizeDeck);

        resizeDeck->addTransition(resizeDeck, SIGNAL(finished()), m_waitState);
    }

    // 3. Map the external events to internal transitions of this state machine.
    auto on_press = new Press_Transition;
    this->addTransition(on_press);
    connect(on_press, &QAbstractTransition::triggered, [&] ()
    {
        auto item = m_sm.presenter().view().scene()->itemAt(m_sm.scenePoint, QTransform());
        if(auto overlay = dynamic_cast<DeckOverlay*>(item))
        {
            m_localSM.postEvent(new ClickOnDeckOverlay_Event{
                                    iscore::IDocument::path(overlay->deckView.presenter.model())});
        }
        else if(auto handle = dynamic_cast<DeckHandle*>(item))
        {
            m_localSM.postEvent(new ClickOnDeckHandle_Event{
                                    iscore::IDocument::path(handle->deckView.presenter.model())});
        }
    });

    // Forward events
    auto on_move = new Move_Transition;
    this->addTransition(on_move);
    connect(on_move, &QAbstractTransition::triggered, [&] ()
    { m_localSM.postEvent(new Move_Event); });

    auto on_release = new Release_Transition;
    this->addTransition(on_release);
    connect(on_release, &QAbstractTransition::triggered, [&] ()
    { m_localSM.postEvent(new Release_Event); });
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

