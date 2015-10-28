#pragma once
#include <QObject>
#include <ProcessInterface/Process.hpp>
#include <memory>

class OSSIAConstraintElement;
namespace OSSIA
{
    class Scenario;
    class TimeProcess;
}

class OSSIAProcessElement : public QObject
{
        Q_OBJECT
    public:
        OSSIAProcessElement(OSSIAConstraintElement& cst, QObject* parent):
            QObject{parent},
            m_parent_constraint{cst}
        {

        }

        virtual std::shared_ptr<OSSIA::TimeProcess> OSSIAProcess() const = 0;
        virtual Process& iscoreProcess() const = 0;
        virtual void stop()
        {
            iscoreProcess().stopExecution();
        }

    signals:
        // Is emitted whenever the implementaton has changed
        // and needs to be put again in the constraint
        void changed(const std::shared_ptr<OSSIA::TimeProcess>&,
                     const std::shared_ptr<OSSIA::TimeProcess>&);

    protected:
        OSSIAConstraintElement& m_parent_constraint;
};
