#pragma once
#include "CurveCommandObjectBase.hpp"
#include <iscore/command/OngoingCommandManager.hpp>

class MovePointCommandObject : public CurveCommandObjectBase
{
    public:
        MovePointCommandObject(CurvePresenter* presenter, iscore::CommandStack& stack);

        void on_press() override;

        void move();

        void release();

        void cancel();

    private:
        SingleOngoingCommandDispatcher m_dispatcher;
};
