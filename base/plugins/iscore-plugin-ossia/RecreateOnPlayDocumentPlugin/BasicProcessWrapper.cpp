#include "BasicProcessWrapper.hpp"

#include <Editor/TimeConstraint.h>
#include <Editor/TimeProcess.h>

#include <algorithm>

namespace RecreateOnPlay
{
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

std::shared_ptr<OSSIA::TimeProcess> BasicProcessWrapper::currentProcess() const
{ return m_process; }

OSSIA::TimeConstraint&BasicProcessWrapper::currentConstraint() const
{ return *m_parent; }
}
