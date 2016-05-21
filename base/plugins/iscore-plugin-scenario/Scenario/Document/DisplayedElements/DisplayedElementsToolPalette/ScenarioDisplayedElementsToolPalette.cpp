#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <iscore/tools/std/Optional.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <algorithm>

#include <Process/TimeValue.hpp>
#include <Scenario/Document/BaseScenario/BaseElementContext.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <iscore/widgets/GraphicsProxyObject.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Palette/Tools/SmartTool.hpp>
#include <Scenario/Palette/Tools/States/ScenarioMoveStatesWrapper.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include "ScenarioDisplayedElementsToolPalette.hpp"

#include <iscore/statemachine/GraphicsSceneToolPalette.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>

namespace Scenario
{
class EditionSettings;

Scenario::Point ScenarioDisplayedElementsToolPalette::ScenePointToScenarioPoint(QPointF point)
{
    return {TimeValue::fromMsecs(point.x() * m_presenter.zoomRatio()) + m_presenter.presenters().startTimeNode().date(), 0};
}

ScenarioDisplayedElementsToolPalette::ScenarioDisplayedElementsToolPalette(
        const DisplayedElementsModel& model,
        ScenarioDocumentPresenter& pres,
        BaseGraphicsObject& view):
    GraphicsSceneToolPalette{*view.scene()},
    m_model{model},
    m_scenarioModel{*safe_cast<Scenario::ScenarioModel*>(m_model.constraint().parent())},
    m_presenter{pres},
    m_context{pres.context(), m_presenter},
    m_view{view},
    m_editionSettings{m_context.context.app.components.applicationPlugin<ScenarioApplicationPlugin>().editionSettings()},
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
}
