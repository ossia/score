#pragma once
#include <iscore/statemachine/ToolState.hpp>
class CurveStateMachine;
class CurveTool : public ToolState
{
    public:
        CurveTool(const CurveStateMachine&);

    protected:
        const CurveStateMachine& m_parentSM;

};
