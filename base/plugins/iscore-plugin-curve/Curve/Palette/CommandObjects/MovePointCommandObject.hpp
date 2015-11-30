#pragma once
#include <Curve/Segment/CurveSegmentData.hpp>
#include "CurveCommandObjectBase.hpp"

class CurvePresenter;
namespace iscore {
class CommandStack;
}  // namespace iscore

class MovePointCommandObject final : public CurveCommandObjectBase
{
    public:
        MovePointCommandObject(CurvePresenter* presenter, iscore::CommandStack& stack);

        void on_press() override;

        void move();

        void release();

        void cancel();

    private:
        void handlePointOverlap(CurveSegmentMap& segments);
        void handleSuppressOnOverlap(CurveSegmentMap& segments);
        void handleCrossOnOverlap(CurveSegmentMap& segments);
        void setCurrentPoint(CurveSegmentMap& segments);

};
