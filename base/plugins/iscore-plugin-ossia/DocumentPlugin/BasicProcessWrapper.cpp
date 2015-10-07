#include "BasicProcessWrapper.hpp"

#include <Editor/TimeConstraint.h>
#include <Editor/TimeProcess.h>

#include <algorithm>

BasicProcessWrapper::BasicProcessWrapper(
        const std::shared_ptr<OSSIA::TimeConstraint>& cst,
        const std::shared_ptr<OSSIA::TimeProcess>& ptr,
        const OSSIA::TimeValue& dur,
        bool looping):
    m_parent{cst},
    m_process{ptr}
{
    if(m_process)
        m_parent->addTimeProcess(m_process);
}

BasicProcessWrapper::~BasicProcessWrapper()
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

void BasicProcessWrapper::setDuration(const OSSIA::TimeValue& val) { }

void BasicProcessWrapper::setLooping(bool b) { }

void BasicProcessWrapper::changed(
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

std::shared_ptr<OSSIA::TimeProcess> BasicProcessWrapper::currentProcess() const
{ return m_process; }

OSSIA::TimeConstraint&BasicProcessWrapper::currentConstraint() const
{ return *m_parent; }
