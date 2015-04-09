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

#include <iscore/document/DocumentInterface.hpp>
MoveDeckToolState::MoveDeckToolState(const ScenarioStateMachine& sm):
    GenericToolStateBase{sm},
    m_dispatcher{m_sm.commandStack(), nullptr}
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


    auto on_press = new Press_Transition;
    this->addTransition(on_press);
    connect(on_press, &QAbstractTransition::triggered, [&] ()
    {
        auto overlay = dynamic_cast<DeckOverlay*>(m_sm.presenter().view().scene()->itemAt(m_sm.scenePoint, QTransform()));
        if(!overlay) return;

        m_sourceDeck = iscore::IDocument::path(overlay->deckView.presenter.model());
    });

    auto on_release = new Release_Transition;
    this->addTransition(on_release);
    connect(on_release, &QAbstractTransition::triggered, [&] ()
    {
        if(m_sourceDeck == ObjectPath{}) return;
        auto overlay = dynamic_cast<DeckOverlay*>(m_sm.presenter().view().scene()->itemAt(m_sm.scenePoint, QTransform()));
        if(!overlay)
        {
            auto cmd = new Scenario::Command::RemoveDeckFromBox(ObjectPath{m_sourceDeck});
            m_dispatcher.submitCommand(cmd);
            return;
        }



        auto& pres = overlay->deckView.presenter;

        //m_sourceDeck = id_type<DeckModel>{};
    });
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

