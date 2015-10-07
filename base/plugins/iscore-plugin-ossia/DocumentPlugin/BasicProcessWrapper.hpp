#pragma once
#include <memory>

namespace OSSIA
{
class TimeConstraint;
class TimeValue;
class TimeProcess;
}
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
