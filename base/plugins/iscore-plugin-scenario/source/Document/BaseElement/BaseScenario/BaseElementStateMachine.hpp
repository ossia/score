#pragma once
#include <iscore/statemachine/BaseStateMachine.hpp>
class BaseElementPresenter;

// TODO MoveMe to statemachine folder
class BaseElementStateMachine: public BaseStateMachine
{
    public:
        BaseElementStateMachine(BaseElementPresenter* pres);

    private:
        BaseElementPresenter* m_presenter;
};

