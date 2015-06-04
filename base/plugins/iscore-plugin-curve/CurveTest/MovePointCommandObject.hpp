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
        void handleLocking(QVector<CurveSegmentModel*>& segments, double current_x, double current_y);
        void handlePointOverlap(QVector<CurveSegmentModel *> &segments, double current_x);
        void handleSuppressOnOverlap(QVector<CurveSegmentModel *> &segments, double current_x);
        void handleCrossOnOverlap(QVector<CurveSegmentModel *> &segments, double current_x);
        void setCurrentPoint(QVector<CurveSegmentModel *> &segments);

        SingleOngoingCommandDispatcher m_dispatcher;
        QVector<QByteArray> m_startSegments;

        double xmin, xmax;
};
