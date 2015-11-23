#include "BaseScenarioStateMachine.hpp"
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/BaseElement/BaseElementView.hpp>

#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/SelectionToolState.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/application/ApplicationContext.hpp>
#include <core/document/Document.hpp>

BaseScenarioToolPalette::BaseScenarioToolPalette(
        BaseElementPresenter& pres):
    GraphicsSceneToolPalette{pres.view().scene()},
    m_presenter{pres},
    m_slotTool{iscore::IDocument::commandStack(m_presenter.model()),
               *this}
{
    //Scenario::SelectionAndMoveTool<BaseScenario, BaseScenarioToolPalette, QGraphicsItem> teupeu(*this);
    con(m_presenter, &BaseElementPresenter::displayedConstraintPressed,
            [=] (const QPointF& point)
    {
        scenePoint = point;
        m_slotTool.on_pressed(point);
    });

    con(m_presenter, &BaseElementPresenter::displayedConstraintMoved,
            [=] (const QPointF& point)
    {
        scenePoint = point;
        m_slotTool.on_moved();
    });

    con(m_presenter, &BaseElementPresenter::displayedConstraintReleased,
            [=] (const QPointF& point)
    {
        scenePoint = point;
        m_slotTool.on_released();
    });
    // TODO cancel

}

QGraphicsItem& BaseScenarioToolPalette::view() const
{
    return *m_presenter.view().baseItem();
}

const DisplayedElementsPresenter&BaseScenarioToolPalette::presenter() const
{
    return m_presenter.presenters();
}

const DisplayedElementsModel& BaseScenarioToolPalette::model() const
{
    return m_presenter.model().displayedElements;
}

const iscore::DocumentContext& BaseScenarioToolPalette::context() const
{
    return iscore::IDocument::documentContext(m_presenter.model());
}

const Scenario::EditionSettings&BaseScenarioToolPalette::editionSettings() const
{
    return context().app.components.applicationPlugin<ScenarioApplicationPlugin>().editionSettings(); // OPTIMIZEME
}
