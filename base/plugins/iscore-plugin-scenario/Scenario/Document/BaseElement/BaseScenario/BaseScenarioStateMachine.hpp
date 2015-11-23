#pragma once
#include <iscore/statemachine/BaseStateMachine.hpp>
#include <Scenario/Document/BaseElement/BaseScenario/StateMachine/BaseMoveSlot.hpp>
class BaseElementPresenter;

// TODO MoveMe to statemachine folder
class BaseScenarioStateMachine final : public GraphicsSceneToolPalette
{
    public:
        BaseScenarioStateMachine(BaseElementPresenter* pres);

    private:
        BaseElementPresenter* m_presenter;
        BaseMoveSlot m_slotTool;
};

