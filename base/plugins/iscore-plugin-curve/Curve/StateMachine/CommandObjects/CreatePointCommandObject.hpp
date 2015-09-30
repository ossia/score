#pragma once
#include "Curve/StateMachine/CommandObjects/CurveCommandObjectBase.hpp"

class CreatePointCommandObject : public CurveCommandObjectBase
{
    public:
        CreatePointCommandObject(CurvePresenter* presenter, iscore::CommandStack& stack);

        void on_press() override;

        void move();

        void release();

        void cancel();

    private:
        void createPoint(std::vector<CurveSegmentData>& segments);
};
