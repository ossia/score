#include "MoveDeckToolState.hpp"
#include "Commands/Constraint/Box/MoveDeck.hpp"
#include "Commands/Constraint/Box/Deck/ResizeDeckVertically.hpp"
#include "Document/Constraint/Box/Deck/DeckView.hpp"
#include "Document/Constraint/Box/Deck/DeckOverlay.hpp"
#include "Document/Constraint/Box/Deck/DeckHandle.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/Deck/DeckPresenter.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "Commands/Constraint/Box/SwapDecks.hpp"
#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"

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

    // Press, move, commit, rollback.. As usual.
    {
        auto dragDeck = new DeckState{&m_localSM};
        {
            // Enter the state
            make_transition<ClickOnDeckOverlay_Transition>(
                        m_waitState,
                        dragDeck,
                        *dragDeck);

            // States :
            auto press = new QState{dragDeck};
            dragDeck->setInitialState(press);
            auto move = new QState{dragDeck};
            auto release = new QFinalState{dragDeck};

            make_transition<Move_Transition>(press, move);
            make_transition<Release_Transition>(press, release);
            make_transition<Release_Transition>(move, release);

            connect(release, &QAbstractState::entered, [=] ( )
            {
                auto overlay = dynamic_cast<DeckOverlay*>(m_sm.presenter().view().scene()->itemAt(m_sm.scenePoint, QTransform()));
                if(overlay)
                {
                    auto& baseDeck = dragDeck->currentDeck.find<DeckModel>();
                    auto& releasedDeck = overlay->deckView.presenter.model();
                    // If it is the same, we do nothing.
                    // If it is another, we swap them
                    if(releasedDeck.id() != baseDeck.id()
                    && releasedDeck.parent() == baseDeck.parent())
                    {
                        auto cmd = new Scenario::Command::SwapDecks{
                                       iscore::IDocument::path(releasedDeck.parent()), // Box
                                       baseDeck.id(), releasedDeck.id()};
                        m_dispatcher.submitCommand(cmd);
                    }
                }
                else
                {
                    // We throw it
                    auto cmd = new Scenario::Command::RemoveDeckFromBox(ObjectPath{dragDeck->currentDeck});
                    m_dispatcher.submitCommand(cmd);
                    return;
                }
            });
        }
        dragDeck->addTransition(dragDeck, SIGNAL(finished()), m_waitState);
    }

    {
        auto resizeDeck = new DeckState{&m_localSM};
        {
            make_transition<ClickOnDeckHandle_Transition>(
                        m_waitState,
                        resizeDeck,
                        *resizeDeck);

            auto press = new QState{resizeDeck};
            resizeDeck->setInitialState(press);
            auto move = new QState{resizeDeck};
            auto release = new QFinalState{resizeDeck};

            make_transition<Move_Transition>(press, move);
            make_transition<Move_Transition>(move, move);
            make_transition<Release_Transition>(press, release);
            make_transition<Release_Transition>(move, release);

            connect(press, &QAbstractState::entered, [=] ()
            {
                m_originalPoint = m_sm.scenePoint;
                m_originalHeight = resizeDeck->currentDeck.find<DeckModel>().height();
            });

            connect(move, &QAbstractState::entered, [=] ( )
            {
                auto val = std::max(20.0,
                                    m_originalHeight + (m_sm.scenePoint.y() - m_originalPoint.y()));

                m_ongoingDispatcher.submitCommand<Scenario::Command::ResizeDeckVertically>(
                            ObjectPath{resizeDeck->currentDeck},
                            val);
                return;
            });

            connect(release, &QAbstractState::entered, [=] ()
            {
                m_ongoingDispatcher.commit();
            });
        }
        resizeDeck->addTransition(resizeDeck, SIGNAL(finished()), m_waitState);
    }

    auto on_press = new Press_Transition;
    this->addTransition(on_press);
    connect(on_press, &QAbstractTransition::triggered, [&] ()
    {
        auto item = m_sm.presenter().view().scene()->itemAt(m_sm.scenePoint, QTransform());
        if(auto overlay = dynamic_cast<DeckOverlay*>(item))
        {
            m_localSM.postEvent(new ClickOnDeckOverlay_Event{
                                    iscore::IDocument::path(overlay->deckView.presenter.model()),
                                    m_sm.scenarioPoint});
        }
        else if(auto handle = dynamic_cast<DeckHandle*>(item))
        {
            m_localSM.postEvent(new ClickOnDeckHandle_Event{
                                    iscore::IDocument::path(handle->deckView.presenter.model()),
                                    m_sm.scenarioPoint});

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

