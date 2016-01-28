#pragma once
#include <vector>

#include <Curve/Palette/CommandObjects/CurveCommandObjectBase.hpp>

namespace iscore {
class CommandStackFacade;
}

namespace Curve
{
struct SegmentData;
class Presenter;

class CreatePointCommandObject final : public CommandObjectBase
{
    public:
        CreatePointCommandObject(
                Presenter* presenter,
                iscore::CommandStackFacade& stack);

        void on_press() override;

        void move();

        void release();

        void cancel();

    private:
        void createPoint(std::vector<SegmentData>& segments);
};
}
