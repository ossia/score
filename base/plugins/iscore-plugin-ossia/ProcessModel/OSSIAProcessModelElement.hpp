#pragma once
#include <DocumentPlugin/OSSIAProcessElement.hpp>
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
