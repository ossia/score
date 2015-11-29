#include "ScenarioDisplayedElementsToolPalette.hpp"

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <core/application/ApplicationContext.hpp>
#include <core/document/Document.hpp>

Scenario::Point ScenarioDisplayedElementsToolPalette::ScenePointToScenarioPoint(QPointF point)
{
    return {TimeValue::fromMsecs(point.x() * m_presenter.zoomRatio()) + m_presenter.presenters().startTimeNode().date(), 0};
}

ScenarioDisplayedElementsToolPalette::ScenarioDisplayedElementsToolPalette(
        const iscore::DocumentContext& ctx,
        const DisplayedElementsModel& model,
        ScenarioDocumentPresenter& pres,
        BaseGraphicsObject& view):
    GraphicsSceneToolPalette{*view.scene()},
    m_model{model},
    m_scenarioModel{*safe_cast<Scenario::ScenarioModel*>(m_model.constraint().parent())},
    m_presenter{pres},
    m_context{ctx, m_presenter},
    m_view{view},
    m_editionSettings{m_context.app.components.applicationPlugin<ScenarioApplicationPlugin>().editionSettings()},
    m_state{*this},
    m_inputDisp{m_presenter, *this, m_context}
{
}

BaseGraphicsObject& ScenarioDisplayedElementsToolPalette::view() const
{
    return m_view;
}

const DisplayedElementsPresenter& ScenarioDisplayedElementsToolPalette::presenter() const
{
    return m_presenter.presenters();
}

const Scenario::ScenarioModel& ScenarioDisplayedElementsToolPalette::model() const
{
    return m_scenarioModel;
}

const BaseElementContext& ScenarioDisplayedElementsToolPalette::context() const
{
    return m_context;
}

const Scenario::EditionSettings& ScenarioDisplayedElementsToolPalette::editionSettings() const
{
    return m_editionSettings;
}


void ScenarioDisplayedElementsToolPalette::activate(Scenario::Tool)
{

}

void ScenarioDisplayedElementsToolPalette::desactivate(Scenario::Tool)
{

}

void ScenarioDisplayedElementsToolPalette::on_pressed(QPointF point)
{
    scenePoint = point;
    m_state.on_pressed(point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
}

void ScenarioDisplayedElementsToolPalette::on_moved(QPointF point)
{
    scenePoint = point;
    m_state.on_moved(point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
}

void ScenarioDisplayedElementsToolPalette::on_released(QPointF point)
{
    scenePoint = point;
    m_state.on_released(point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
}

void ScenarioDisplayedElementsToolPalette::on_cancel()
{
    m_state.on_cancel();
}
