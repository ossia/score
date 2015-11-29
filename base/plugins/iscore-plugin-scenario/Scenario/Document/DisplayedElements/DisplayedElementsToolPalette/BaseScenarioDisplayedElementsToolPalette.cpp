#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentView.hpp>
#include <boost/core/explicit_operator_bool.hpp>
#include <boost/optional/optional.hpp>
#include <core/application/ApplicationContext.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <algorithm>

#include <Scenario/Document/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Document/ScenarioDocument/ProcessFocusManager.hpp>
#include "BaseScenarioDisplayedElementsToolPalette.hpp"
#include "Process/TimeValue.hpp"
#include "Scenario/Document/BaseScenario/BaseElementContext.hpp"
#include "Scenario/Document/BaseScenario/BaseScenario.hpp"
#include "Scenario/Document/DisplayedElements/DisplayedElementsToolPalette/BaseScenarioDisplayedElements_StateWrappers.hpp"
#include "Scenario/Document/ScenarioDocument/Widgets/GraphicsProxyObject.hpp"
#include "core/application/ApplicationComponents.hpp"
#include "iscore/statemachine/GraphicsSceneToolPalette.hpp"
#include "iscore/tools/SettableIdentifier.hpp"

namespace Scenario {
class EditionSettings;
}  // namespace Scenario

Scenario::Point BaseScenarioDisplayedElementsToolPalette::ScenePointToScenarioPoint(QPointF point)
{
    return {TimeValue::fromMsecs(point.x() * m_presenter.zoomRatio()) , 0};
}

// We need two tool palettes : one for the case where we're viewing a basescenario,
// and one for the case where we're in a sub-scenario.
BaseScenarioDisplayedElementsToolPalette::BaseScenarioDisplayedElementsToolPalette(
        ScenarioDocumentPresenter& pres):
    GraphicsSceneToolPalette{pres.view().scene()},
    m_presenter{pres},
    m_context{iscore::IDocument::documentContext(m_presenter.model()), m_presenter},
    m_state{*this},
    m_inputDisp{m_presenter, *this, m_context}
{
}

BaseGraphicsObject& BaseScenarioDisplayedElementsToolPalette::view() const
{
    return *m_presenter.view().baseItem();
}

const DisplayedElementsPresenter&BaseScenarioDisplayedElementsToolPalette::presenter() const
{
    return m_presenter.presenters();
}

const BaseScenario& BaseScenarioDisplayedElementsToolPalette::model() const
{
    return m_presenter.model().baseScenario();
}

const BaseElementContext& BaseScenarioDisplayedElementsToolPalette::context() const
{
    return m_context;
}

const Scenario::EditionSettings& BaseScenarioDisplayedElementsToolPalette::editionSettings() const
{
    return m_context.app.components.applicationPlugin<ScenarioApplicationPlugin>().editionSettings(); // OPTIMIZEME
}

void BaseScenarioDisplayedElementsToolPalette::activate(Scenario::Tool)
{

}

void BaseScenarioDisplayedElementsToolPalette::desactivate(Scenario::Tool)
{

}

void BaseScenarioDisplayedElementsToolPalette::on_pressed(QPointF point)
{
    scenePoint = point;
    m_state.on_pressed(point, ScenePointToScenarioPoint(m_presenter.view().baseItem()->mapFromScene(point)));
}

void BaseScenarioDisplayedElementsToolPalette::on_moved(QPointF point)
{
    scenePoint = point;
    m_state.on_moved(point, ScenePointToScenarioPoint(m_presenter.view().baseItem()->mapFromScene(point)));
}

void BaseScenarioDisplayedElementsToolPalette::on_released(QPointF point)
{
    scenePoint = point;
    m_state.on_released(point, ScenePointToScenarioPoint(m_presenter.view().baseItem()->mapFromScene(point)));
}

void BaseScenarioDisplayedElementsToolPalette::on_cancel()
{
    m_state.on_cancel();
}
