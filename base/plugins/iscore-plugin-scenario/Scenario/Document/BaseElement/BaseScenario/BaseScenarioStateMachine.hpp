#pragma once
#include <iscore/statemachine/BaseStateMachine.hpp>
class BaseElementPresenter;

// TODO MoveMe to statemachine folder
class BaseScenarioStateMachine final : public BaseStateMachine
{
    public:
        BaseScenarioStateMachine(BaseElementPresenter* pres);

    private:
        BaseElementPresenter* m_presenter;
};

