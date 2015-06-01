#include "BaseMoveDeck.hpp"
#include "Process/Temporal/StateMachines/Tools/States/DeckStates.hpp"
#include "Commands/Constraint/Box/MoveDeck.hpp"
#include "Commands/Constraint/Box/Deck/ResizeDeckVertically.hpp"
#include "Document/Constraint/Box/Deck/DeckView.hpp"
#include "Document/Constraint/Box/Deck/DeckOverlay.hpp"
#include "Document/Constraint/Box/Deck/DeckHandle.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/Deck/DeckPresenter.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "Commands/Constraint/Box/SwapDecks.hpp"
#include "Process/Temporal/StateMachines/Transitions/DeckTransitions.hpp"

#include <QFinalState>
#include <QGraphicsScene>
#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"

BaseMoveDeck::BaseMoveDeck(
        const QGraphicsScene& scene,
        iscore::CommandStack& stack,
        BaseStateMachine& sm):
    QState{&sm},
    m_sm{sm},
    m_dispatcher{stack},
    m_ongoingDispatcher{stack},
    m_scene{scene}
{
    /// 2. Setup the sub-state machine.
    m_waitState = new QState{&m_localSM};
    m_localSM.setInitialState(m_waitState);
    // Two states : one for moving the content of the deck, one for resizing with the handle.
    {
        auto dragDeck = new DragDeckState{m_dispatcher, m_sm, m_scene, &m_localSM};
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
    connect(on_press, &QAbstractTransition::triggered, this, [&] ()
    {
        auto item = m_scene.itemAt(m_sm.scenePoint, QTransform());
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

    m_localSM.start();
}
