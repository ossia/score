#pragma once
#include <vector>

#include <Curve/Palette/CommandObjects/CurveCommandObjectBase.hpp>

class CurvePresenter;
namespace iscore {
class CommandStackFacade;
}  // namespace iscore
struct CurveSegmentData;

class CreatePointCommandObject final : public CurveCommandObjectBase
{
    public:
        CreatePointCommandObject(
                CurvePresenter* presenter,
                iscore::CommandStackFacade& stack);

        void on_press() override;

        void move();

        void release();

        void cancel();

    private:
        void createPoint(std::vector<CurveSegmentData>& segments);
};
