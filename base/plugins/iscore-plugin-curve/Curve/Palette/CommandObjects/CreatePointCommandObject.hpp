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

class ISCORE_PLUGIN_CURVE_EXPORT CreatePointCommandObject final : public CommandObjectBase
{
    public:
        CreatePointCommandObject(
                Presenter* presenter,
                const iscore::CommandStackFacade& stack);
        virtual ~CreatePointCommandObject();

        void on_press() override;

        void move();

        void release();

        void cancel();

    private:
        void createPoint(std::vector<SegmentData>& segments);
};
}
