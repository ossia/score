#pragma once
#include <iscore/component/Component.hpp>
#include <Process/Process.hpp>

namespace Scenario
{
// TODO namespace process instead?

template<typename Component_T>
class ProcessComponent :
        public Component_T
{
    public:
        template<typename... Args>
        ProcessComponent(Process::ProcessModel& cst, Args&&... args):
            Component_T{std::forward<Args>(args)...},
            m_process{cst}
        {

        }

        Process::ProcessModel& process() const
        { return m_process; }

    private:
        Process::ProcessModel& m_process;
};

template<typename System_T>
using GenericProcessComponent =
    Scenario::ProcessComponent<iscore::GenericComponent<System_T>>;
}
