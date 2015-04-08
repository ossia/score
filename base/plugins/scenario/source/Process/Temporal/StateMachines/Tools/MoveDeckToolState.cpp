#include "MoveDeckToolState.hpp"
#include "Commands/Constraint/Box/MoveDeck.hpp"
#include "Document/Constraint/Box/Deck/DeckView.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "Process/Temporal/TemporalScenarioView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/Box/BoxPresenter.hpp"
#include "Document/Constraint/Box/Deck/DeckPresenter.hpp"
MoveDeckToolState::MoveDeckToolState(const ScenarioStateMachine& sm):
    GenericToolStateBase{sm}
{
    // TODO check the enablement of a deck when it is created.
    connect(this, &QState::entered,
            [&] ()
    {
        qDebug() << "da";
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
        qDebug() << "do";
        for(TemporalConstraintPresenter* constraint : m_sm.presenter().constraints())
        {
            if(!constraint->box()) continue;
            for(DeckPresenter* deck : constraint->box()->decks())
            {
                deck->enable();
            }
        }
    });



}

void MoveDeckToolState::on_scenarioPressed()
{
    qDebug() << dynamic_cast<DeckView*>(m_sm.presenter().view().scene()->itemAt(m_sm.scenePoint, QTransform()));
}

void MoveDeckToolState::on_scenarioMoved()
{

}

void MoveDeckToolState::on_scenarioReleased()
{

}

