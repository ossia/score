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


class LoopingProcessWrapper
{
    public:
        LoopingProcessWrapper(const std::shared_ptr<OSSIA::TimeConstraint>& cst,
                       const std::shared_ptr<OSSIA::TimeProcess>& ptr,
                       const OSSIA::TimeValue& dur,
                       bool looping);

        ~LoopingProcessWrapper();

        void setDuration(const OSSIA::TimeValue& val);

        void setLooping(bool b);

        void changed(const std::shared_ptr<OSSIA::TimeProcess>&,
                     const std::shared_ptr<OSSIA::TimeProcess>&);

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
