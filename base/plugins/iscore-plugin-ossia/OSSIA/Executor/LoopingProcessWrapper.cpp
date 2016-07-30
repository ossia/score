#include <ossia/editor/loop/loop.hpp>
#include <ossia/editor/scenario/scenario.hpp>
#include <ossia/editor/scenario/time_constraint.hpp>
#include <ossia/editor/scenario/time_node.hpp>
#include <vector>

#include <ossia/editor/scenario/time_value.hpp>
#include "LoopingProcessWrapper.hpp"

namespace RecreateOnPlay
{
static void loopingConstraintCallback(
        ossia::time_value,
        ossia::time_value t,
        const ossia::State& element)
{
}
LoopingProcessWrapper::LoopingProcessWrapper(
        const std::shared_ptr<ossia::time_constraint>& cst,
        const std::shared_ptr<ossia::time_process>& ptr,
        ossia::time_value dur,
        bool looping):
    m_parent{cst},
    m_process{ptr},
    m_fixed_impl{ossia::scenario::create()},
    m_fixed_endNode{ossia::time_node::create()},
    m_looping_impl{ossia::loop::create(dur, loopingConstraintCallback, {}, {})},
    m_looping{looping}
{
    auto sev = m_fixed_impl->getStartTimeNode()->emplace(
          m_fixed_impl->getStartTimeNode()->timeEvents().begin(),
          {}, ossia::expressions::make_expression_true());

    auto eev = m_fixed_endNode->emplace(m_fixed_endNode->timeEvents().begin(), {}, ossia::expressions::make_expression_true());
    m_fixed_impl->addTimeNode(m_fixed_endNode);

    m_fixed_cst = ossia::time_constraint::create(loopingConstraintCallback,
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

std::shared_ptr<ossia::time_process> LoopingProcessWrapper::currentProcess() const
{
    if(m_looping)
        return m_looping_impl;
    else
        return m_fixed_impl;
}

ossia::time_constraint& LoopingProcessWrapper::currentConstraint() const
{
    if(m_looping)
        return *m_looping_impl->getPatternTimeConstraint();
    else
        return *m_fixed_cst;
}
}
