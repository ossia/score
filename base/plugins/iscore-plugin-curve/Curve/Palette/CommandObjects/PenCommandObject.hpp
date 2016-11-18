#pragma once
#include <iscore/tools/std/Optional.hpp>
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
#include <QPoint>
#include <Curve/Palette/CommandObjects/CurveCommandObjectBase.hpp>
#include <Curve/Commands/UpdateCurve.hpp>
#include <Curve/Segment/PointArray/PointArraySegment.hpp>

namespace Curve
{
class Model;
class Presenter;
class StateBase;
class ISCORE_PLUGIN_CURVE_EXPORT PenCommandObject final : public CommandObjectBase
{
    public:
        PenCommandObject(
                Presenter* presenter,
                const iscore::CommandStackFacade&);

        void setCurveState(Curve::StateBase* stateBase) { m_state = stateBase; }

        void on_press() override;

        void move();

        void release();

        void cancel();

    private:
        using seg_tuple = std::tuple<
            optional<SegmentData>,
            optional<SegmentData>,
            std::vector<SegmentData>>;
        void release_n(seg_tuple&&);
        seg_tuple filterSegments();
        PointArraySegment m_segment;

        Curve::StateBase* m_state{};
        QPointF m_originalPress;
        QPointF m_minPress{}, m_maxPress{};
};
}
