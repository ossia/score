#pragma once
#include <memory>

namespace OSSIA
{
class Loop;
class Scenario;
class TimeConstraint;
class TimeNode;
class TimeProcess;
class TimeValue;
}

namespace RecreateOnPlay
{

class LoopingProcessWrapper
{
    public:
        LoopingProcessWrapper(const std::shared_ptr<OSSIA::TimeConstraint>& cst,
                       const std::shared_ptr<OSSIA::TimeProcess>& ptr,
                       const OSSIA::TimeValue& dur,
                       bool looping);

    private:
        std::shared_ptr<OSSIA::TimeProcess> currentProcess() const;
        OSSIA::TimeConstraint& currentConstraint() const;

        std::shared_ptr<OSSIA::TimeConstraint> m_parent;
        std::shared_ptr<OSSIA::TimeProcess> m_process;

        std::shared_ptr<OSSIA::Scenario> m_fixed_impl;
        std::shared_ptr<OSSIA::TimeNode> m_fixed_endNode;
        std::shared_ptr<OSSIA::TimeConstraint> m_fixed_cst;

        std::shared_ptr<OSSIA::Loop> m_looping_impl;
        bool m_looping = false;
};
}
