#include <ossia/editor/scenario/time_constraint.hpp>
#include <ossia/editor/scenario/time_process.hpp>

#include "BasicProcessWrapper.hpp"
#include <ossia/editor/scenario/time_value.hpp>

namespace RecreateOnPlay
{
BasicProcessWrapper::BasicProcessWrapper(
        const std::shared_ptr<ossia::time_constraint>& cst,
        std::unique_ptr<ossia::time_process> ptr,
        ossia::time_value dur,
        bool looping):
    m_parent{cst},
    m_process{*ptr}
{
    m_parent->addTimeProcess(std::move(ptr));
}

}
