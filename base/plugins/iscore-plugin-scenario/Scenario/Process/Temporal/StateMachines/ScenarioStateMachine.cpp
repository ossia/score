#include "ScenarioStateMachine.hpp"
#include <iscore/statemachine/StateMachineTools.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioPresenter.hpp>
#include <Scenario/Process/Temporal/TemporalScenarioView.hpp>

#include <core/document/Document.hpp>
#include <iscore/document/DocumentInterface.hpp>

#include <Scenario/Control/ScenarioControl.hpp>
#include <QSignalTransition>
#include <core/application/Application.hpp>

namespace Scenario
{
ToolPalette::ToolPalette(
        iscore::Document& doc,
        TemporalScenarioPresenter& presenter):
    GraphicsSceneToolPalette{*presenter.view().scene()},
    m_presenter{presenter},
    m_model{static_cast<const ScenarioModel&>(m_presenter.m_layer.processModel())},
    m_commandStack{doc.commandStack()},
    m_locker{doc.locker()},
    m_createTool{*this},
    m_selectTool{*this},
    m_moveSlotTool{*this}
{
    auto ScenePointToScenarioPoint = [&] (QPointF point) -> Scenario::Point
    {
        return ConvertToScenarioPoint(
                    point,
                    m_presenter.zoomRatio(),
                    m_presenter.view().boundingRect().height());
    };
    connect(m_presenter.m_view, &TemporalScenarioView::scenarioPressed,
            [=] (QPointF point)
    {
        scenePoint = point;
        auto scenarioPoint = ScenePointToScenarioPoint(m_presenter.m_view->mapFromScene(point));
        switch(editionSettings().tool())
        {
            case Scenario::Tool::Create:
                m_createTool.start();
                m_createTool.on_pressed(point, scenarioPoint);
                break;
            case Scenario::Tool::Select:
                m_selectTool.start();
                m_selectTool.on_pressed(point, scenarioPoint);
                break;
            case Scenario::Tool::MoveSlot:
                m_moveSlotTool.start();
                m_moveSlotTool.on_pressed(point);
                break;
            default:
                break;
        }
    });

    connect(m_presenter.m_view, &TemporalScenarioView::scenarioReleased,
            [=] (QPointF point)
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
    });

    connect(m_presenter.m_view, &TemporalScenarioView::scenarioMoved,
            [=] (QPointF point)
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
    });

    connect(m_presenter.m_view, &TemporalScenarioView::escPressed,
            [=] ()
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
    });

    con(editionSettings(), &Scenario::EditionSettings::toolChanged,
            this, &ToolPalette::changeTool);
    changeTool(editionSettings().tool());
}

const Scenario::EditionSettings&ToolPalette::editionSettings() const
{
    return m_presenter.editionSettings();
}

void ToolPalette::changeTool(Scenario::Tool state)
{
    m_createTool.stop();
    m_selectTool.stop();
    m_moveSlotTool.stop();

    switch(state)
    {
        case Scenario::Tool::Create:
            m_createTool.start();
            break;
        case Scenario::Tool::Select:
            m_selectTool.start();
            break;
        case Scenario::Tool::MoveSlot:
            m_moveSlotTool.start();
            break;
        case Scenario::Tool::Disabled:
        case Scenario::Tool::Playing:
            break;
        default:
            ISCORE_ABORT;
            break;
    }
}
}
