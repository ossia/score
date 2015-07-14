#include "BaseScenarioStateMachine.hpp"
#include "StateMachine/BaseMoveSlot.hpp"
#include "Document/BaseElement/BaseElementModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "Document/BaseElement/BaseElementView.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

BaseScenarioStateMachine::BaseScenarioStateMachine(BaseElementPresenter* pres):
    BaseStateMachine{*pres->view()->scene()},
    m_presenter{pres}
{
    connect(m_presenter, &BaseElementPresenter::displayedConstraintPressed,
            [=] (const QPointF& point)
    {
        scenePoint = point;
        this->postEvent(new Press_Event);
    });

    connect(m_presenter, &BaseElementPresenter::displayedConstraintMoved,
            [=] (const QPointF& point)
    {
        scenePoint = point;
        this->postEvent(new Move_Event);
    });

    connect(m_presenter, &BaseElementPresenter::displayedConstraintReleased,
            [=] (const QPointF& point)
    {
        scenePoint = point;
        this->postEvent(new Release_Event);
    });
    // TODO cancel

    auto moveSlotState = new BaseMoveSlot(*m_presenter->view()->scene(),
                                           iscore::IDocument::documentFromObject(m_presenter->model())->commandStack(),
                                           *this);
    setInitialState(moveSlotState);
    start();
}
