#include "BaseScenarioStateMachine.hpp"
#include "StateMachine/BaseMoveSlot.hpp"
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/BaseElement/BaseElementView.hpp>
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

BaseScenarioStateMachine::BaseScenarioStateMachine(BaseElementPresenter* pres):
    BaseStateMachine{*pres->view().scene()},
    m_presenter{pres}
{
    connect(m_presenter, &BaseElementPresenter::displayedConstraintPressed,
            [=] (const QPointF& point)
    {
        scenePoint = point;
        this->postEvent(new iscore::Press_Event);
    });

    connect(m_presenter, &BaseElementPresenter::displayedConstraintMoved,
            [=] (const QPointF& point)
    {
        scenePoint = point;
        this->postEvent(new iscore::Move_Event);
    });

    connect(m_presenter, &BaseElementPresenter::displayedConstraintReleased,
            [=] (const QPointF& point)
    {
        scenePoint = point;
        this->postEvent(new iscore::Release_Event);
    });
    // TODO cancel

    auto moveSlotState = new BaseMoveSlot(*m_presenter->view().scene(),
                                           iscore::IDocument::commandStack(m_presenter->model()),
                                           *this);
    setInitialState(moveSlotState);
    start();
}
