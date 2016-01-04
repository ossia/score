#pragma once
#include <OSSIA/Executor/ProcessElement.hpp>
#include <OSSIA/Executor/ConstraintElement.hpp>
#include "ProcessModel.hpp"

namespace RecreateOnPlay
{
class ProcessModelElement : public ProcessElement
{
    public:
        ProcessModelElement(
                ConstraintElement& cst,
                OSSIAProcessModel& proc,
                QObject* parent):
            ProcessElement{cst, parent},
            m_process{proc}
        {
            m_process.process()->setParentConstraint(cst.OSSIAConstraint());
        }

        std::shared_ptr<OSSIA::TimeProcess> OSSIAProcess() const override
        {
            return m_process.process();
        }

        OSSIAProcessModel& iscoreProcess() const override
        {
            return m_process;
        }

    private:
        OSSIAProcessModel& m_process;
};
}
