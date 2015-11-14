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
    createTool{*this},
    selectTool{*this},
    moveSlotTool{*this}
{
    connect(&model(), &Process::execution,
            this, [&] (bool b) {
            changeTool(b ? ScenarioToolKind::Play
                         : ScenarioToolKind::Select);
    });

    auto QPointFToScenarioPoint = [&] (QPointF point) -> Scenario::Point
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
        auto scenarioPoint = QPointFToScenarioPoint(m_presenter.m_view->mapFromScene(point));
        switch(editionSettings().tool())
        {
            case ScenarioToolKind::Create:
                createTool.on_pressed(point, scenarioPoint);
                break;
            case ScenarioToolKind::Select:
                selectTool.on_pressed(point, scenarioPoint);
                break;
            case ScenarioToolKind::MoveSlot:
                moveSlotTool.on_pressed(point);
                break;
            case ScenarioToolKind::Play:
                break;
        }
    });
    connect(m_presenter.m_view, &TemporalScenarioView::scenarioReleased,
            [=] (QPointF point)
    {
        scenePoint = point;
        auto scenarioPoint = QPointFToScenarioPoint(m_presenter.m_view->mapFromScene(point));
        switch(editionSettings().tool())
        {
            case ScenarioToolKind::Create:
                createTool.start();
                createTool.on_released(point, scenarioPoint);
                break;
            case ScenarioToolKind::Select:
                selectTool.start();
                selectTool.on_released(point, scenarioPoint);
                break;
            case ScenarioToolKind::MoveSlot:
                moveSlotTool.start();
                moveSlotTool.on_released();
                break;
            case ScenarioToolKind::Play:
                break;
        }
    });
    connect(m_presenter.m_view, &TemporalScenarioView::scenarioMoved,
            [=] (QPointF point)
    {
        scenePoint = point;
        auto scenarioPoint = QPointFToScenarioPoint(m_presenter.m_view->mapFromScene(point));
        switch(editionSettings().tool())
        {
            case ScenarioToolKind::Create:
                createTool.on_moved(point, scenarioPoint);
                break;
            case ScenarioToolKind::Select:
                selectTool.on_moved(point, scenarioPoint);
                break;
            case ScenarioToolKind::MoveSlot:
                moveSlotTool.on_moved();
                break;
            case ScenarioToolKind::Play:
                break;
        }
    });
    connect(m_presenter.m_view, &TemporalScenarioView::escPressed,
            [=] ()
    {
        switch(editionSettings().tool())
        {
            case ScenarioToolKind::Create:
                createTool.on_cancel();
                break;
            case ScenarioToolKind::Select:
                selectTool.on_cancel();
                break;
            case ScenarioToolKind::MoveSlot:
                moveSlotTool.on_cancel();
                break;
            case ScenarioToolKind::Play:
                break;
        }
    });

    connect(&editionSettings(), &ScenarioEditionSettings::toolChanged,
            this, &ToolPalette::changeTool);
    changeTool(editionSettings().tool());
}

const ScenarioEditionSettings&ToolPalette::editionSettings() const
{
    return m_presenter.editionSettings();
}

void ToolPalette::changeTool(ScenarioToolKind state)
{
    createTool.stop();
    selectTool.stop();
    moveSlotTool.stop();

    switch(state)
    {
        case ScenarioToolKind::Create:
        {
            createTool.start();
            break;
        }
        case ScenarioToolKind::Select:
        {
            selectTool.start();
            break;
        }
        case ScenarioToolKind::MoveSlot:
        {
            moveSlotTool.start();
            break;
        }
        case ScenarioToolKind::Play:
        {
            break;


        }
        default:
            ISCORE_ABORT;
            break;
    }
}
}
