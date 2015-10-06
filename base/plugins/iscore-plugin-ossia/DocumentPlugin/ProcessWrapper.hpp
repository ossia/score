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

// MOVEME
class BasicProcessWrapper
{

    public:
        BasicProcessWrapper(const std::shared_ptr<OSSIA::TimeConstraint>& cst,
                       const std::shared_ptr<OSSIA::TimeProcess>& ptr,
                       const OSSIA::TimeValue& dur,
                       bool looping);

        ~BasicProcessWrapper();

        void setDuration(const OSSIA::TimeValue& val);
        void setLooping(bool b);

        void changed(const std::shared_ptr<OSSIA::TimeProcess>& oldProc,
                     const std::shared_ptr<OSSIA::TimeProcess>& newProc);

    private:
        std::shared_ptr<OSSIA::TimeProcess> currentProcess() const;
        OSSIA::TimeConstraint& currentConstraint() const;

        std::shared_ptr<OSSIA::TimeConstraint> m_parent;
        std::shared_ptr<OSSIA::TimeProcess> m_process;
};


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

using ProcessWrapper = LoopingProcessWrapper;

struct OSSIAProcess
{
        OSSIAProcess() = default;
        OSSIAProcess(OSSIAProcess&&) = default;
        OSSIAProcess& operator=(OSSIAProcess&&) = default;

        OSSIAProcess(OSSIAProcessElement* e, std::unique_ptr<ProcessWrapper>&& proc):
            element(e),
            wrapper(std::move(proc))
        {

        }

        OSSIAProcessElement* element{};
        std::unique_ptr<ProcessWrapper> wrapper;
};
