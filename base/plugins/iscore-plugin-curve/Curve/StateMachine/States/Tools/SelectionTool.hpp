#pragma once
#include <iscore/selection/SelectionDispatcher.hpp>
#include "CurveTool.hpp"
class QState;

namespace Curve
{
class SelectionState;
class SelectionAndMoveTool final : public CurveTool
{
    public:
        explicit SelectionAndMoveTool(CurveStateMachine& sm);

        void on_pressed() override;
        void on_moved() override;
        void on_released() override;

    private:
        Curve::SelectionState* m_state{};
};
}
