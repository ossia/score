#pragma once
#include "Curve/StateMachine/States/Tools/CurveTool.hpp"
#include <QTimer>
namespace Curve
{

class OngoingState;

class EditionTool : public CurveTool
{
        Q_OBJECT
    public:
        explicit EditionTool(CurveStateMachine& sm);
        enum class Mode { Create, Move, SetSegment };

        void changeMode(int state);
        int mode() const;

    signals:
        void setCreationState();
        void setMoveState();
        void setSetSegmentState();

        void exitState();

    protected:
        void on_pressed() final override;
        void on_moved() final override;
        void on_released() final override;

    private:
        std::chrono::steady_clock::time_point m_prev;
};

class CreateTool final : public EditionTool
{
    public:
        explicit CreateTool(CurveStateMachine& sm);
};
class MoveTool final : public EditionTool
{
    public:
        explicit MoveTool(CurveStateMachine& sm);
};
class SetSegmentTool final : public EditionTool
{
    public:
        explicit SetSegmentTool(CurveStateMachine& sm);
};

}
