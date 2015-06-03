#include "MoveTool.hpp"
#include "CurveTest/StateMachine/States/Move/MovePointState.hpp"
#include "CurveTest/StateMachine/States/Move/MoveSegmentState.hpp"
#include "CurveTest/CurveModel.hpp"
#include "CurveTest/OngoingCommandState.hpp"
#include <iscore/statemachine/StateMachineUtils.hpp>
#include <iscore/document/DocumentInterface.hpp>

using namespace Curve;
class MovePointCommandObject
{
        CurveStateMachine& m_sm;
    public:
        MovePointCommandObject(CurveStateMachine& sm):
            m_sm{sm}
        {

        }

        void press()
        {

        }

        void move()
        {

        }

        void release()
        {

        }

        void cancel(){

        }
};

MoveTool::MoveTool(CurveStateMachine& sm):
    CurveTool{sm, &sm}
{
    m_waitState = new QState;
    localSM().addState(m_waitState);
    localSM().setInitialState(m_waitState);

    /// Point
    auto mpco = new MovePointCommandObject(sm);
    m_movePoint = new OngoingState(*mpco, &localSM());
    /*
    m_movePoint =
            new MovePointState{
                  m_parentSM,
                  iscore::IDocument::path(m_parentSM.model()),
                  m_parentSM.commandStack(),
                  m_parentSM.locker(),
                  nullptr};

    make_transition<ClickOnPoint_Transition>(
                m_waitState,
                m_movePoint,
                *m_movePoint);
    m_movePoint->addTransition(
                m_movePoint,
                SIGNAL(finished()),
                m_waitState);

    localSM().addState(m_movePoint);

    /// Segment
    m_moveSegment =
            new MoveSegmentState{
                  m_parentSM,
                  iscore::IDocument::path(m_parentSM.model()),
                  m_parentSM.commandStack(),
                  m_parentSM.locker(),
                  nullptr};

    make_transition<ClickOnSegmentTransition>(
                m_waitState,
                m_moveSegment,
                *m_moveSegment);
    m_moveSegment->addTransition(
                m_moveSegment,
                SIGNAL(finished()),
                m_waitState);


    localSM().addState(m_moveSegment);
    */

    localSM().start();
}
