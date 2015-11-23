#include "FullViewStateMachine.hpp"

#include <Scenario/Document/BaseElement/DisplayedElements/DisplayedElementsPresenter.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Document/BaseElement/BaseElementPresenter.hpp>
#include <Scenario/Document/BaseElement/DisplayedElements/DisplayedElementsModel.hpp>
#include <Scenario/Document/BaseElement/DisplayedElements/DisplayedElementsPresenter.hpp>
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
        const BaseElementPresenter& pres,
        BaseGraphicsObject& view):
    GraphicsSceneToolPalette{*view.scene()},
    m_context{ctx},
    m_model{model},
    m_presenter{pres},
    m_view{view},
    m_editionSettings{m_context.app.components.applicationPlugin<ScenarioApplicationPlugin>().editionSettings()},
    m_state{*this}
{

     con(m_presenter, &BaseElementPresenter::pressed,
         this, [=] (QPointF point)
     {
         scenePoint = point;
         m_state.on_pressed(point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
     });

     con(m_presenter, &BaseElementPresenter::moved,
         this, [=] (QPointF point)
     {
         scenePoint = point;
         m_state.on_moved(point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
     });

     con(m_presenter, &BaseElementPresenter::released,
         this, [=] (QPointF point)
     {
         scenePoint = point;
         m_state.on_released(point, ScenePointToScenarioPoint(m_view.mapFromScene(point)));
     });
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
    return *safe_cast<ScenarioModel*>(m_model.displayedConstraint().parentScenario());
}

const iscore::DocumentContext& FullViewToolPalette::context() const
{
    return m_context;
}

const Scenario::EditionSettings&FullViewToolPalette::editionSettings() const
{
    return m_editionSettings;
}
