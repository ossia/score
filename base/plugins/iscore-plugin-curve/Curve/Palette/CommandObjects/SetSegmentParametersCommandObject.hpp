#pragma once
#include <boost/optional/optional.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <QPoint>

#include <Curve/Commands/SetSegmentParameters.hpp>

namespace iscore {
class CommandStackFacade;
}  // namespace iscore

namespace Curve
{
class Presenter;
class StateBase;
class SetSegmentParametersCommandObject
{
    public:
        SetSegmentParametersCommandObject(Presenter* pres, iscore::CommandStackFacade&);

        void setCurveState(Curve::StateBase* stateBase) { m_state = stateBase; }

        void press();

        void move();

        void release();

        void cancel();

    private:
        Presenter* m_presenter{};
        SingleOngoingCommandDispatcher<SetSegmentParameters> m_dispatcher;

        Curve::StateBase* m_state{};
        QPointF m_originalPress;
        boost::optional<double> m_verticalOrig, m_horizontalOrig;
};
}
