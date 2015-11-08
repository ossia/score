#pragma once
#include <DocumentPlugin/OSSIAProcessElement.hpp>
#include <DocumentPlugin/OSSIAConstraintElement.hpp>
#include "OSSIAProcessModel.hpp"

class OSSIAProcessModelElement : public OSSIAProcessElement
{
    public:
        OSSIAProcessModelElement(
                OSSIAConstraintElement& cst,
                OSSIAProcessModel& proc,
                QObject* parent):
            OSSIAProcessElement{cst, parent},
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

        void recreate() override
        {
            m_process.recreate();
        }

        void clear() override
        {
            m_process.clear();
        }

    private:
        OSSIAProcessModel& m_process;
};
