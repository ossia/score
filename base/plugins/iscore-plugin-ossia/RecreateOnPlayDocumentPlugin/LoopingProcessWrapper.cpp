#include <Editor/Loop.h>
#include <Editor/Scenario.h>
#include <Editor/TimeConstraint.h>
#include <Editor/TimeNode.h>
#include <vector>

#include "Editor/TimeValue.h"
#include "LoopingProcessWrapper.hpp"

namespace RecreateOnPlay
{
static void loopingConstraintCallback(const OSSIA::TimeValue&,
                               const OSSIA::TimeValue& t,
                               std::shared_ptr<OSSIA::StateElement> element)
{
}
LoopingProcessWrapper::LoopingProcessWrapper(
        const std::shared_ptr<OSSIA::TimeConstraint>& cst,
        const std::shared_ptr<OSSIA::TimeProcess>& ptr,
        const OSSIA::TimeValue& dur,
        bool looping):
    m_parent{cst},
    m_process{ptr},
    m_fixed_impl{OSSIA::Scenario::create()},
    m_fixed_endNode{OSSIA::TimeNode::create()},
    m_looping_impl{OSSIA::Loop::create(dur, loopingConstraintCallback, {}, {})},
    m_looping{looping}
{
    auto sev = m_fixed_impl->getStartTimeNode()->emplace(m_fixed_impl->getStartTimeNode()->timeEvents().begin(), {});

    auto eev = m_fixed_endNode->emplace(m_fixed_endNode->timeEvents().begin(), {});
    m_fixed_impl->addTimeNode(m_fixed_endNode);

    m_fixed_cst = OSSIA::TimeConstraint::create(loopingConstraintCallback,
                       *sev, *eev, dur, dur, dur);

    m_fixed_impl->addTimeConstraint(m_fixed_cst);

    if(m_looping)
    {
        // Move the process from the scenario to the loop
        if(m_process)
            m_looping_impl->getPatternTimeConstraint()->addTimeProcess(m_process);
        m_parent->addTimeProcess(m_looping_impl);
    }
    else
    {
        // Move the process from the loop to the scenario
        if(m_process)
            m_fixed_cst->addTimeProcess(m_process);
        m_parent->addTimeProcess(m_fixed_impl);
    }
}

std::shared_ptr<OSSIA::TimeProcess> LoopingProcessWrapper::currentProcess() const
{
    if(m_looping)
        return m_looping_impl;
    else
        return m_fixed_impl;
}

OSSIA::TimeConstraint& LoopingProcessWrapper::currentConstraint() const
{
    if(m_looping)
        return *m_looping_impl->getPatternTimeConstraint();
    else
        return *m_fixed_cst;
}
}
