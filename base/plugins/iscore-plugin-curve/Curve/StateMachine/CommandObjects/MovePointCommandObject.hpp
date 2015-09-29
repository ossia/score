#pragma once
#include "CurveCommandObjectBase.hpp"

class MovePointCommandObject : public CurveCommandObjectBase
{
    public:
        MovePointCommandObject(CurvePresenter* presenter, iscore::CommandStack& stack);

        void on_press() override;

        void move();

        void release();

        void cancel();

    private:
        void handlePointOverlap(QVector<CurveSegmentData> &segments);
        void handleSuppressOnOverlap(QVector<CurveSegmentData> &segments);
        void handleCrossOnOverlap(QVector<CurveSegmentData> &segments);
        void setCurrentPoint(QVector<CurveSegmentData> &segments);

};
