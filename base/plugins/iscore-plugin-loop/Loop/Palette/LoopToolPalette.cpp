#include "LoopToolPalette.hpp"
#include <Loop/LoopPresenter.hpp>

LoopToolPalette::LoopToolPalette(
        const Loop::ProcessModel& model,
        LoopPresenter& presenter,
        LayerContext& ctx,
        LoopView& view):
    GraphicsSceneToolPalette{*view.scene()},
    m_model{model},
    m_presenter{presenter},
    m_context{ctx},
    m_view{view},
    m_editionSettings{m_context.app.components.applicationPlugin<ScenarioApplicationPlugin>().editionSettings()},
    m_state{*this},
    m_inputDisp{m_presenter, *this, m_context}
{

}

LoopView&LoopToolPalette::view() const
{
    return m_view;
}

const LoopPresenter&LoopToolPalette::presenter() const
{
    return m_presenter;
}

const Loop::ProcessModel&LoopToolPalette::model() const
{
    return m_model;
}

const LayerContext&LoopToolPalette::context() const
{
    return m_context;
}

const Scenario::EditionSettings&LoopToolPalette::editionSettings() const
{
    return m_editionSettings;
}

void LoopToolPalette::activate(Scenario::Tool)
{

}

void LoopToolPalette::desactivate(Scenario::Tool)
{

}

void LoopToolPalette::on_pressed(QPointF point)
{
    scenePoint = point;
    auto scenarioPoint = ScenePointToScenarioPoint(m_view.mapFromScene(point));
    m_state.on_pressed(point, scenarioPoint);
}

void LoopToolPalette::on_moved(QPointF point)
{
    scenePoint = point;
    auto scenarioPoint = ScenePointToScenarioPoint(m_view.mapFromScene(point));
    m_state.on_moved(point, scenarioPoint);
}

void LoopToolPalette::on_released(QPointF point)
{
    scenePoint = point;
    auto scenarioPoint = ScenePointToScenarioPoint(m_view.mapFromScene(point));
    m_state.on_released(point, scenarioPoint);
}

void LoopToolPalette::on_cancel()
{
    m_state.on_cancel();
}

Scenario::Point LoopToolPalette::ScenePointToScenarioPoint(QPointF point)
{
    return {TimeValue::fromMsecs(point.x() * m_presenter.zoomRatio()) , 0};
}
