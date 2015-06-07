#pragma once
#include "Curve/StateMachine/CommandObjects/CurveCommandObjectBase.hpp"

class CreatePointFromNothingCommandObject : public CurveCommandObjectBase
{
    public:
        CreatePointFromNothingCommandObject(CurvePresenter* presenter, iscore::CommandStack& stack);

        void on_press() override;

        void move();

        void release();

        void cancel();

    private:
        void createPoint(QVector<CurveSegmentModel *>& segments);
};
