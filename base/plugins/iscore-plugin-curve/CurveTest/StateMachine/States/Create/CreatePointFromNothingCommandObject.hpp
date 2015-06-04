#pragma once
#include "CurveTest/CurveCommandObjectBase.hpp"
#include <iscore/command/OngoingCommandManager.hpp>

class CreatePointFromNothingCommandObject : public CurveCommandObjectBase
{
    public:
        CreatePointFromNothingCommandObject(CurvePresenter* presenter, iscore::CommandStack& stack);

        void on_press() override;

        void move();

        void release();

        void cancel();

    private:
        SingleOngoingCommandDispatcher m_dispatcher;
        QVector<QByteArray> m_startSegments;
        void createPoint(QVector<CurveSegmentModel *>& segments);
};
