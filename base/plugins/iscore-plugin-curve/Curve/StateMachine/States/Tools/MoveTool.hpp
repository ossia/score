#pragma once
#include "Curve/StateMachine/States/Tools/CurveTool.hpp"
namespace Curve
{

class OngoingState;

class CreateTool final : public Curve::EditionToolForCreate
{
    public:
        explicit CreateTool(CurveStateMachine& sm);
};
class SetSegmentTool final : public Curve::EditionToolForCreate
{
    public:
        explicit SetSegmentTool(CurveStateMachine& sm);
};

}
