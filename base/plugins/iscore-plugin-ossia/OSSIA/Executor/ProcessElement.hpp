#pragma once
#include <QObject>
#include <Process/Process.hpp>
#include <memory>

namespace OSSIA
{
    class Scenario;
    class TimeProcess;
}

namespace RecreateOnPlay
{
class ConstraintElement;
class ProcessElement : public QObject
{
        Q_OBJECT
    public:
        ProcessElement(ConstraintElement& cst, QObject* parent):
            QObject{parent},
            m_parent_constraint{cst}
        {

        }

        virtual std::shared_ptr<OSSIA::TimeProcess> OSSIAProcess() const = 0;
        virtual Process::ProcessModel& iscoreProcess() const = 0;
        virtual void stop()
        {
            iscoreProcess().stopExecution();
        }

    protected:
        ConstraintElement& m_parent_constraint;
};
}
