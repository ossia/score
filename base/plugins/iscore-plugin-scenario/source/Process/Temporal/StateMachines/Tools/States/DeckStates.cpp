#include "DeckStates.hpp"

#include "Process/Temporal/StateMachines/ScenarioStateMachine.hpp"
#include "Process/Temporal/StateMachines/ScenarioStateMachineBaseTransitions.hpp"

#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Document/Constraint/Box/Deck/DeckOverlay.hpp"
#include "Document/Constraint/Box/Deck/DeckView.hpp"
#include "Document/Constraint/Box/Deck/DeckPresenter.hpp"

#include "Commands/Constraint/Box/Deck/ResizeDeckVertically.hpp"
#include "Commands/Constraint/Box/MoveDeck.hpp"
#include "Commands/Constraint/Box/SwapDecks.hpp"

#include <QFinalState>
#include <QGraphicsScene>
ResizeDeckState::ResizeDeckState(
        SingleOngoingCommandDispatcher &dispatcher,
        const BaseStateMachine &sm,
        QState *parent):
    DeckState{parent},
    m_ongoingDispatcher{dispatcher},
    m_sm{sm}
{
    auto press = new QState{this};
    this->setInitialState(press);
    auto move = new QState{this};
    auto release = new QFinalState{this};

    make_transition<Move_Transition>(press, move);
    make_transition<Move_Transition>(move, move);
    make_transition<Release_Transition>(press, release);
    make_transition<Release_Transition>(move, release);

    connect(press, &QAbstractState::entered, [=] ()
    {
        m_originalPoint = m_sm.scenePoint;
        m_originalHeight = this->currentDeck.find<DeckModel>().height();
    });

    connect(move, &QAbstractState::entered, [=] ( )
    {
        auto val = std::max(20.0,
                            m_originalHeight + (m_sm.scenePoint.y() - m_originalPoint.y()));

        m_ongoingDispatcher.submitCommand<Scenario::Command::ResizeDeckVertically>(
                    ObjectPath{this->currentDeck},
                    val);
        return;
    });

    connect(release, &QAbstractState::entered, [=] ()
    {
        m_ongoingDispatcher.commit();
    });
}


DragDeckState::DragDeckState(CommandDispatcher<> &dispatcher,
        const BaseStateMachine &sm,
        const QGraphicsScene &scene,
        QState *parent):
    DeckState{parent},
    m_dispatcher{dispatcher},
    m_sm{sm},
    m_scene{scene}
{
    // States :
    auto press = new QState{this};
    this->setInitialState(press);
    auto move = new QState{this};
    auto release = new QFinalState{this};

    make_transition<Move_Transition>(press, move);
    make_transition<Release_Transition>(press, release);
    make_transition<Release_Transition>(move, release);

    connect(release, &QAbstractState::entered, [=] ( )
    {
        auto overlay = dynamic_cast<DeckOverlay*>(m_scene.itemAt(m_sm.scenePoint, QTransform()));
        if(overlay)
        {
            auto& baseDeck = this->currentDeck.find<DeckModel>();
            auto& releasedDeck = overlay->deckView().presenter.model();
            // If it is the same, we do nothing.
            // If it is another (in the same box), we swap them
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
            auto cmd = new Scenario::Command::RemoveDeckFromBox(ObjectPath{this->currentDeck});
            m_dispatcher.submitCommand(cmd);
            return;
        }
    });
}
