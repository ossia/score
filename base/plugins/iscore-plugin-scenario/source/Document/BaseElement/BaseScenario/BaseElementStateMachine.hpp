#pragma once
#include
class BaseElementStateMachine: public BaseStateMachine
{
        BaseElementPresenter* m_presenter;
    public:
        BaseElementStateMachine(BaseElementPresenter* pres);
};

BaseElementStateMachine::BaseElementStateMachine(BaseElementPresenter* pres):
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
                                           IDocument::documentFromObject(m_presenter->model())->commandStack(),
                                           *this);
    setInitialState(moveSlotState);
    start();
}
