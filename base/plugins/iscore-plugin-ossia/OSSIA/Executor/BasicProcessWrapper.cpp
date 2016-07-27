#include <ossia/editor/scenario/time_constraint.hpp>

#include "BasicProcessWrapper.hpp"
#include <ossia/editor/scenario/time_value.hpp>

namespace RecreateOnPlay
{
BasicProcessWrapper::BasicProcessWrapper(
        const std::shared_ptr<OSSIA::TimeConstraint>& cst,
        const std::shared_ptr<OSSIA::TimeProcess>& ptr,
        OSSIA::TimeValue dur,
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
