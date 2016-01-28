#pragma once
#include <memory>

namespace OSSIA
{
class TimeConstraint;
class TimeValue;
class TimeProcess;
}

namespace RecreateOnPlay
{
class BasicProcessWrapper
{
    public:
        BasicProcessWrapper(const std::shared_ptr<OSSIA::TimeConstraint>& cst,
                       const std::shared_ptr<OSSIA::TimeProcess>& ptr,
                       const OSSIA::TimeValue& dur,
                       bool looping);

    private:
        std::shared_ptr<OSSIA::TimeProcess> currentProcess() const;
        OSSIA::TimeConstraint& currentConstraint() const;

        std::shared_ptr<OSSIA::TimeConstraint> m_parent;
        std::shared_ptr<OSSIA::TimeProcess> m_process;
};
}
