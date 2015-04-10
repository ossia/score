#include "MoveDeckToolState.hpp"
#include "Commands/Constraint/Box/MoveDeck.hpp"
#include "Commands/Constraint/Box/RemoveDeckFromBox.hpp"
#include "Document/Constraint/Box/Deck/DeckView.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/Deck/DeckPresenter.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"

#include "Process/Temporal/StateMachines/StateMachineCommon.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <QFinalState>

MoveDeckToolState::MoveDeckToolState(const ScenarioStateMachine& sm):
    GenericStateBase{sm},
    m_dispatcher{m_sm.commandStack(), nullptr}
{
    /// 1. Set the scenario in the correct state with regards to this tool.
    // TODO check the enablement of a deck when it is created.
    connect(this, &QState::entered,
            [&] ()
    {
        for(TemporalConstraintPresenter* constraint : m_sm.presenter().constraints())
        {
            if(!constraint->box()) continue;
            for(DeckPresenter* deck : constraint->box()->decks())
            {
                deck->disable();
            }
        }
    });
    connect(this, &QState::exited,
            [&] ()
    {
        for(TemporalConstraintPresenter* constraint : m_sm.presenter().constraints())
        {
            if(!constraint->box()) continue;
            for(DeckPresenter* deck : constraint->box()->decks())
            {
                deck->enable();
            }
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
            auto t_wait_click =
                    make_transition<ClickOnDeck_Transition>(
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
                if(!overlay)
                {
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
            auto t_wait_click =
                    make_transition<ClickOnDeckHandle_Transition>(
                        m_waitState,
                        resizeDeck,
                        *resizeDeck);



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
            m_localSM.postEvent(new ClickOnDeck_Event{
                                    iscore::IDocument::path(overlay->deckView.presenter.model()),
                                    m_sm.scenarioPoint});
        }
        else if(auto deckview = dynamic_cast<DeckView*>(item))
        {
            m_localSM.postEvent(new ClickOnDeckHandle_Event{
                                    iscore::IDocument::path(deckview->presenter.model()),
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

