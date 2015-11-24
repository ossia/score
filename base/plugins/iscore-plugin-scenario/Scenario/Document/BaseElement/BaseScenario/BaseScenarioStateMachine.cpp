#include "BaseScenarioStateMachine.hpp"
#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/BaseElement/BaseElementView.hpp>

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <core/application/ApplicationContext.hpp>
#include <core/document/Document.hpp>

Scenario::Point BaseScenarioToolPalette::ScenePointToScenarioPoint(QPointF point)
{
    return {TimeValue::fromMsecs(point.x() * m_presenter.zoomRatio()) , 0};
}

// We need two tool palettes : one for the case where we're viewing a basescenario,
// and one for the case where we're in a sub-scenario.
BaseScenarioToolPalette::BaseScenarioToolPalette(
        BaseElementPresenter& pres):
    GraphicsSceneToolPalette{pres.view().scene()},
    m_presenter{pres},
    m_context{iscore::IDocument::documentContext(m_presenter.model()), m_presenter},
    m_state{*this},
    m_inputDisp{m_presenter, *this, m_context}
{
    // TODO cancel

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

const BaseElementContext& BaseScenarioToolPalette::context() const
{
    return m_context;
}

const Scenario::EditionSettings&BaseScenarioToolPalette::editionSettings() const
{
    return m_context.app.components.applicationPlugin<ScenarioApplicationPlugin>().editionSettings(); // OPTIMIZEME
}

void BaseScenarioToolPalette::activate(Scenario::Tool)
{

}

void BaseScenarioToolPalette::desactivate(Scenario::Tool)
{

}

void BaseScenarioToolPalette::on_pressed(QPointF point)
{
    scenePoint = point;
    m_state.on_pressed(point, ScenePointToScenarioPoint(m_presenter.view().baseItem()->mapFromScene(point)));
}

void BaseScenarioToolPalette::on_moved(QPointF point)
{
    scenePoint = point;
    m_state.on_moved(point, ScenePointToScenarioPoint(m_presenter.view().baseItem()->mapFromScene(point)));
}

void BaseScenarioToolPalette::on_released(QPointF point)
{
    scenePoint = point;
    m_state.on_released(point, ScenePointToScenarioPoint(m_presenter.view().baseItem()->mapFromScene(point)));
}

void BaseScenarioToolPalette::on_cancel()
{
    m_state.on_cancel();
}
