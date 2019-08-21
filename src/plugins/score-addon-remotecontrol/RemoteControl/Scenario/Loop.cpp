#include "Loop.hpp"

#include <Scenario/Process/Algorithms/Accessors.hpp>
namespace RemoteControl
{

LoopBase::LoopBase(
    ::Loop::ProcessModel& scenario,
    DocumentPlugin& doc,
    const Id<score::Component>& id,
    QObject* parent_obj)
    : ProcessComponent_T<Loop::ProcessModel>{scenario,
                                             doc,
                                             id,
                                             "LoopComponent",
                                             parent_obj}
{
}

}
