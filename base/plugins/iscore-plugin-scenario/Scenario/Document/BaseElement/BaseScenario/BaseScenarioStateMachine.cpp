#include "BaseScenarioStateMachine.hpp"
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/BaseElement/BaseElementView.hpp>

#include <Scenario/Document/BaseElement/BaseScenario/BaseScenario.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/application/ApplicationContext.hpp>
#include <core/document/Document.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/SelectionToolState.hpp>

// We need two tool palettes : one for the case where we're viewing a basescenario,
// and one for the case where we're in a sub-scenario.
BaseScenarioToolPalette::BaseScenarioToolPalette(
        const BaseElementPresenter& pres):
    GraphicsSceneToolPalette{pres.view().scene()},
    m_presenter{pres},
    m_slotTool{iscore::IDocument::commandStack(m_presenter.model()),
               *this}
{
    //Scenario::SelectionAndMoveTool<BaseScenario, BaseScenarioToolPalette, BaseGraphicsObject, void, MoveBaseEvent>  m_state(*this);
/*
    con(m_presenter, &BaseElementPresenter::pressed,
        this, [=] (QPointF point)
    {
        scenePoint = point;
        m_slotTool.on_pressed(point);
    });

    con(m_presenter, &BaseElementPresenter::moved,
        this, [=] (QPointF point)
    {
        scenePoint = point;
        m_slotTool.on_moved();
    });

    con(m_presenter, &BaseElementPresenter::released,
        this, [=] (QPointF point)
    {
        scenePoint = point;
        m_slotTool.on_released();
    });
    // TODO cancel
*/
}

BaseGraphicsObject& BaseScenarioToolPalette::view() const
{
    return *m_presenter.view().baseItem();
}

const DisplayedElementsPresenter&BaseScenarioToolPalette::presenter() const
{
    return m_presenter.presenters();
}

const BaseScenario& BaseScenarioToolPalette::model() const
{
    return m_presenter.model().baseScenario();
}

const iscore::DocumentContext& BaseScenarioToolPalette::context() const
{
    return iscore::IDocument::documentContext(m_presenter.model());
}

const Scenario::EditionSettings&BaseScenarioToolPalette::editionSettings() const
{
    return context().app.components.applicationPlugin<ScenarioApplicationPlugin>().editionSettings(); // OPTIMIZEME
}













