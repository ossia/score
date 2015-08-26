#pragma once
#include "Curve/StateMachine/States/Tools/CurveTool.hpp"

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
        void on_pressed();
        void on_moved();
        void on_released();
};

class CreateTool : public EditionTool
{
    public:
        explicit CreateTool(CurveStateMachine& sm);
};
class MoveTool : public EditionTool
{
    public:
        explicit MoveTool(CurveStateMachine& sm);
};
class SetSegmentTool : public EditionTool
{
    public:
        explicit SetSegmentTool(CurveStateMachine& sm);
};

}
