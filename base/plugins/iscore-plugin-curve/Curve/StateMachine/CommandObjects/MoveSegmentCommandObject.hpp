#pragma once
#include <iscore/command/Dispatchers/SingleOngoingCommandDispatcher.hpp>
// To simplify :
// Take the current state of the curve
// Compute the state we have to be in
// Make a command that sets a new state for the curve.


// Will move the segment and potentially the start point of the next segment and the end point of the previous segment.
// Or does the command do this ?
// How to find the previous - next segments ? They have to be linked...
// How to prevent overlapping segments ?
// Can move create new segments ? I'd say no.
class MoveSegmentCommandObject
{
    public:
        MoveSegmentCommandObject(iscore::CommandStack& stack);

        void press();

        void move();

        void release();

        void cancel();

    private:
        SingleOngoingCommandDispatcher m_dispatcher;
};
