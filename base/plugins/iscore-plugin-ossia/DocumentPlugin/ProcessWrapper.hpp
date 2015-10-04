#pragma once
#include <memory>

class ConstraintModel;
class Process;
class OSSIAProcessElement;

namespace OSSIA
{
class Loop;
class TimeConstraint;
class TimeValue;
class StateElement;
class Scenario;
class TimeNode;
class TimeProcess;
}

class ProcessWrapper
{
    public:
        ProcessWrapper(const std::shared_ptr<OSSIA::TimeConstraint>& cst,
                       const std::shared_ptr<OSSIA::TimeProcess>& ptr,
                       const OSSIA::TimeValue& dur,
                       bool looping);

        ~ProcessWrapper();

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

struct OSSIAProcess
{
        OSSIAProcessElement* element{};
        ProcessWrapper wrapper;
};
