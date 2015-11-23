#include "ScenarioStateMachine.hpp"
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>


#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <core/application/Application.hpp>

namespace Scenario
{
ToolPalette::ToolPalette(
        LayerContext& lay,
        TemporalScenarioPresenter& presenter):
    GraphicsSceneToolPalette{*presenter.view().scene()},
    m_presenter{presenter},
    m_model{static_cast<const ScenarioModel&>(m_presenter.m_layer.processModel())},
    m_context{lay},
    m_createTool{*this},
    m_selectTool{*this},
    m_moveSlotTool{*this},
    m_inputDisp{presenter.view(), *this, lay}
{
}

const Scenario::EditionSettings&ToolPalette::editionSettings() const
{
    return m_presenter.editionSettings();
}

void ToolPalette::on_pressed(QPointF point)
{
    scenePoint = point;
    auto scenarioPoint = ScenePointToScenarioPoint(m_presenter.m_view->mapFromScene(point));
    switch(editionSettings().tool())
    {
        case Scenario::Tool::Create:
            m_createTool.on_pressed(point, scenarioPoint);
            break;
        case Scenario::Tool::Select:
            m_selectTool.on_pressed(point, scenarioPoint);
            break;
        case Scenario::Tool::MoveSlot:
            m_moveSlotTool.on_pressed(point);
            break;
        default:
            break;
    }

}

void ToolPalette::on_moved(QPointF point)
{
    scenePoint = point;
    auto scenarioPoint = ScenePointToScenarioPoint(m_presenter.m_view->mapFromScene(point));
    switch(editionSettings().tool())
    {
        case Scenario::Tool::Create:
            m_createTool.on_moved(point, scenarioPoint);
            break;
        case Scenario::Tool::Select:
            m_selectTool.on_moved(point, scenarioPoint);
            break;
        case Scenario::Tool::MoveSlot:
            m_moveSlotTool.on_moved();
            break;
        default:
            break;
    }

}

void ToolPalette::on_released(QPointF point)
{
    scenePoint = point;
    auto scenarioPoint = ScenePointToScenarioPoint(m_presenter.m_view->mapFromScene(point));
    switch(editionSettings().tool())
    {
        case Scenario::Tool::Create:
            m_createTool.on_released(point, scenarioPoint);
            break;
        case Scenario::Tool::Select:
            m_selectTool.on_released(point, scenarioPoint);
            break;
        case Scenario::Tool::MoveSlot:
            m_moveSlotTool.on_released();
            break;
        default:
            break;
    }

}

void ToolPalette::on_cancel()
{
    switch(editionSettings().tool())
    {
        case Scenario::Tool::Create:
            m_createTool.on_cancel();
            break;
        case Scenario::Tool::Select:
            m_selectTool.on_cancel();
            break;
        case Scenario::Tool::MoveSlot:
            m_moveSlotTool.on_cancel();
            break;
        default:
            break;
    }
}

void ToolPalette::activate(Tool t)
{
    if(t == Scenario::Tool::MoveSlot)
        m_moveSlotTool.activate();
}

void ToolPalette::desactivate(Tool t)
{
    if(t == Scenario::Tool::MoveSlot)
        m_moveSlotTool.desactivate();
}

Scenario::Point ToolPalette::ScenePointToScenarioPoint(QPointF point)
{
    return ConvertToScenarioPoint(
                point,
                m_presenter.zoomRatio(),
                m_presenter.view().boundingRect().height());
}
}
