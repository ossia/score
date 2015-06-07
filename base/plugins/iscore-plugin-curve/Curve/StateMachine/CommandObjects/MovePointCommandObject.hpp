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
        void handlePointOverlap(QVector<CurveSegmentModel *> &segments);
        void handleSuppressOnOverlap(QVector<CurveSegmentModel *> &segments);
        void handleCrossOnOverlap(QVector<CurveSegmentModel *> &segments);
        void setCurrentPoint(QVector<CurveSegmentModel *> &segments);

};
