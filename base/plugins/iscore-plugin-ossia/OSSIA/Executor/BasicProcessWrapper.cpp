#include <ossia/editor/scenario/time_constraint.hpp>

#include "BasicProcessWrapper.hpp"
#include <ossia/editor/scenario/time_value.hpp>

namespace RecreateOnPlay
{
BasicProcessWrapper::BasicProcessWrapper(
        const std::shared_ptr<ossia::time_constraint>& cst,
        const std::shared_ptr<ossia::time_process>& ptr,
        ossia::time_value dur,
        bool looping):
    m_parent{cst},
    m_process{ptr}
{
    if(m_process)
        m_parent->addTimeProcess(m_process);
}

std::shared_ptr<ossia::time_process> BasicProcessWrapper::currentProcess() const
{ return m_process; }

ossia::time_constraint&BasicProcessWrapper::currentConstraint() const
{ return *m_parent; }
}
