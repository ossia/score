#pragma once
#include <Scenario/Document/BaseElement/BaseScenario/StateMachine/BaseMoveSlot.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Document/BaseElement/Widgets/GraphicsProxyObject.hpp>
#include <Scenario/Process/Temporal/StateMachines/Tools/SelectionToolState.hpp>

#include <iscore/statemachine/BaseStateMachine.hpp>

class BaseElementPresenter;
class BaseGraphicsObject;
class DisplayedElementsPresenter;
class DisplayedElementsModel;
class ScenarioModel;

// RENAME FILE
class FullViewToolPalette final : public GraphicsSceneToolPalette
{
    public:
        FullViewToolPalette(
                const iscore::DocumentContext& ctx,
                const DisplayedElementsModel&,
                const BaseElementPresenter&,
                BaseGraphicsObject&);

        BaseGraphicsObject& view() const;
        const DisplayedElementsPresenter& presenter() const;
        const ScenarioModel& model() const;
        const iscore::DocumentContext& context() const;
        const Scenario::EditionSettings& editionSettings() const;

    private:
        Scenario::Point ScenePointToScenarioPoint(QPointF point);
        const iscore::DocumentContext& m_context;
        const DisplayedElementsModel& m_model;
        const BaseElementPresenter& m_presenter;
        BaseGraphicsObject& m_view;
        const Scenario::EditionSettings& m_editionSettings;

        Scenario::SelectionAndMoveTool<ScenarioModel, FullViewToolPalette, BaseGraphicsObject>  m_state;
};

