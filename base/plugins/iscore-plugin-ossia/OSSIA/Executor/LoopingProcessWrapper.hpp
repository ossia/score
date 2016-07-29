#pragma once
#include <memory>
#include <OSSIA/Executor/ProcessWrapper.hpp>

namespace ossia
{
class loop;
class scenario;
class time_constraint;
class time_node;
class time_process;
class time_value;
}

namespace RecreateOnPlay
{

class LoopingProcessWrapper : public ProcessWrapper
{
    public:
        LoopingProcessWrapper(const std::shared_ptr<ossia::time_constraint>& cst,
                       const std::shared_ptr<ossia::time_process>& ptr,
                       ossia::time_value dur,
                       bool looping);

    private:
        std::shared_ptr<ossia::time_process> currentProcess() const;
        ossia::time_constraint& currentConstraint() const;

        std::shared_ptr<ossia::time_constraint> m_parent;
        std::shared_ptr<ossia::time_process> m_process;

        std::shared_ptr<ossia::scenario> m_fixed_impl;
        std::shared_ptr<ossia::time_node> m_fixed_endNode;
        std::shared_ptr<ossia::time_constraint> m_fixed_cst;

        std::shared_ptr<ossia::loop> m_looping_impl;
        bool m_looping = false;
};
}
