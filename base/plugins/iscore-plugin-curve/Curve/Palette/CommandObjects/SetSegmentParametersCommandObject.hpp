#pragma once
#include <boost/optional/optional.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <QPoint>

#include <Curve/Commands/SetSegmentParameters.hpp>

class CurvePresenter;
namespace Curve {
class StateBase;
}  // namespace Curve
namespace iscore {
class CommandStack;
}  // namespace iscore

class SetSegmentParametersCommandObject
{
    public:
        SetSegmentParametersCommandObject(CurvePresenter* pres, iscore::CommandStack&);

        void setCurveState(Curve::StateBase* stateBase) { m_state = stateBase; }

        void press();

        void move();

        void release();

        void cancel();

    private:
        CurvePresenter* m_presenter{};
        SingleOngoingCommandDispatcher<SetSegmentParameters> m_dispatcher;

        Curve::StateBase* m_state{};
        QPointF m_originalPress;
        boost::optional<double> m_verticalOrig, m_horizontalOrig;
};
