#pragma once
#include <iscore/statemachine/BaseStateMachine.hpp>
#include <Scenario/Document/BaseElement/BaseScenario/StateMachine/BaseMoveSlot.hpp>
#include <Scenario/Application/ScenarioEditionSettings.hpp>
class DisplayedElementsPresenter;
class DisplayedElementsModel;
class BaseElementPresenter;
class ConstraintModel;

// TODO MoveMe to statemachine folder
// TODO rename me
class BaseScenario;
class QGraphicsItem;

class BaseScenarioToolPalette final : public GraphicsSceneToolPalette
{
    public:
        BaseScenarioToolPalette(const BaseElementPresenter& pres);

        QGraphicsItem& view() const;
        const DisplayedElementsPresenter& presenter() const;
        const DisplayedElementsModel& model() const;
        const iscore::DocumentContext& context() const;
        const Scenario::EditionSettings& editionSettings() const;

    private:
        const BaseElementPresenter& m_presenter;
        BaseMoveSlot m_slotTool;
};

