#include "BaseScenarioStateMachine.hpp"
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/BaseElement/BaseElementView.hpp>
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

BaseScenarioStateMachine::BaseScenarioStateMachine(
        BaseElementPresenter* pres):
    GraphicsSceneToolPalette{*pres->view().scene()},
    m_presenter{pres},
    m_slotTool{*m_presenter->view().scene(),
               iscore::IDocument::commandStack(m_presenter->model()),
               *this}
{
    connect(m_presenter, &BaseElementPresenter::displayedConstraintPressed,
            [=] (const QPointF& point)
    {
        scenePoint = point;
        m_slotTool.on_pressed(point);
    });

    connect(m_presenter, &BaseElementPresenter::displayedConstraintMoved,
            [=] (const QPointF& point)
    {
        scenePoint = point;
        m_slotTool.on_moved();
    });

    connect(m_presenter, &BaseElementPresenter::displayedConstraintReleased,
            [=] (const QPointF& point)
    {
        scenePoint = point;
        m_slotTool.on_released();
    });
    // TODO cancel

}
