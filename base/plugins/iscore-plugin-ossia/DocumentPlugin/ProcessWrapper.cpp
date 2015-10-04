#include "ProcessWrapper.hpp"
#include <Editor/Scenario.h>
#include <Editor/Loop.h>
#include <Editor/TimeEvent.h>
#include <Editor/TimeNode.h>
#include <Editor/TimeConstraint.h>
#include <algorithm>
ProcessWrapper::ProcessWrapper(
        const std::shared_ptr<OSSIA::TimeConstraint>& cst,
        const std::shared_ptr<OSSIA::TimeProcess>& ptr,
        const OSSIA::TimeValue& dur,
        bool looping):
    m_parent{cst},
    m_process{ptr},
    m_fixed_impl{OSSIA::Scenario::create()},
    m_fixed_endNode{OSSIA::TimeNode::create()},
    m_looping_impl{OSSIA::Loop::create(dur, {}, {}, {})},
    m_looping{looping}
{
    auto sev = m_fixed_impl->getStartTimeNode()->emplace(m_fixed_impl->getStartTimeNode()->timeEvents().begin(), {});

    auto eev = m_fixed_endNode->emplace(m_fixed_endNode->timeEvents().begin(), {});
    m_fixed_impl->addTimeNode(m_fixed_endNode);

    m_fixed_cst = OSSIA::TimeConstraint::create({}, *sev, *eev, dur);

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

ProcessWrapper::~ProcessWrapper()
{
    auto proc = currentProcess();
    auto& processes = m_parent->timeProcesses();
    auto proc_it =  std::find(processes.begin(),
                              processes.end(),
                              currentProcess());

    // It is possible for a process to be null
    // (e.g. invalid state in GUI like automation without address)
    if(proc && proc_it != processes.end())
        m_parent->removeTimeProcess(proc);
}

void ProcessWrapper::setDuration(const OSSIA::TimeValue& val)
{
    m_fixed_cst->setDuration(val);
    m_looping_impl->getPatternTimeConstraint()->setDuration(val);
}

void ProcessWrapper::setLooping(bool b)
{
    if(b == m_looping)
        return;

    m_looping = b;

    if(m_looping)
    {
        // Move the process from the scenario to the loop
        if(m_process)
        {
            m_fixed_cst->removeTimeProcess(m_process);
            m_looping_impl->getPatternTimeConstraint()->addTimeProcess(m_process);
        }

        m_parent->removeTimeProcess(m_fixed_impl);
        m_parent->addTimeProcess(m_looping_impl);
    }
    else
    {
        // Move the process from the loop to the scenario
        if(m_process)
        {
            m_looping_impl->getPatternTimeConstraint()->removeTimeProcess(m_process);
            m_fixed_cst->addTimeProcess(m_process);
        }

        m_parent->removeTimeProcess(m_looping_impl);
        m_parent->addTimeProcess(m_fixed_impl);
    }

}

void ProcessWrapper::changed(
        const std::shared_ptr<OSSIA::TimeProcess>& oldProc,
        const std::shared_ptr<OSSIA::TimeProcess>& newProc)
{
    if(oldProc)
    {
        currentConstraint().removeTimeProcess(oldProc);
    }

    m_process = newProc;
    if(m_process)
    {
        currentConstraint().addTimeProcess(m_process);
    }
}

std::shared_ptr<OSSIA::TimeProcess> ProcessWrapper::currentProcess() const
{
    if(m_looping)
        return m_looping_impl;
    else
        return m_fixed_impl;
}

OSSIA::TimeConstraint& ProcessWrapper::currentConstraint() const
{
    if(m_looping)
        return *m_looping_impl->getPatternTimeConstraint();
    else
        return *m_fixed_cst;
}
