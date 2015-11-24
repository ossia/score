#include "FullViewStateMachine.hpp"

#include <Scenario/Document/BaseElement/BaseElementModel.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/BaseElement/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/BaseElement/DisplayedElements/DisplayedElementsPresenter.hpp>

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioPoint.hpp>

#include <core/application/ApplicationContext.hpp>
#include <core/document/Document.hpp>

Scenario::Point FullViewToolPalette::ScenePointToScenarioPoint(QPointF point)
{
    return {TimeValue::fromMsecs(point.x() * m_presenter.zoomRatio()) + m_presenter.presenters().startTimeNode().date(), 0};
}

FullViewToolPalette::FullViewToolPalette(
        const iscore::DocumentContext& ctx,
        const DisplayedElementsModel& model,
        BaseElementPresenter& pres,
        BaseGraphicsObject& view):
    GraphicsSceneToolPalette{*view.scene()},
    m_model{model},
    m_presenter{pres},
    m_context{ctx, m_presenter},
    m_view{view},
    m_editionSettings{m_context.app.components.applicationPlugin<ScenarioApplicationPlugin>().editionSettings()},
    m_state{*this},
    m_inputDisp{m_presenter, *this, m_context}
{
}

BaseGraphicsObject& FullViewToolPalette::view() const
{
    return m_view;
}

const DisplayedElementsPresenter& FullViewToolPalette::presenter() const
{
    return m_presenter.presenters();
}

const ScenarioModel& FullViewToolPalette::model() const
{
    return *safe_cast<ScenarioModel*>(m_model.constraint().parentScenario());
}

const BaseElementContext& FullViewToolPalette::context() const
{
    return m_context;
}

const Scenario::EditionSettings&FullViewToolPalette::editionSettings() const
{
    return m_editionSettings;
}


void FullViewToolPalette::activate(Scenario::Tool)
{

}

void FullViewToolPalette::desactivate(Scenario::Tool)
{

}

void FullViewToolPalette::on_pressed(QPointF point)
{
    scenePoint = point;
    m_state.on_pressed(point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
}

void FullViewToolPalette::on_moved(QPointF point)
{
    scenePoint = point;
    m_state.on_moved(point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
}

void FullViewToolPalette::on_released(QPointF point)
{
    scenePoint = point;
    m_state.on_released(point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
}

void FullViewToolPalette::on_cancel()
{
    m_state.on_cancel();
}
