#pragma once
#include <QObject>
#include <ProcessInterface/Process.hpp>
#include <memory>

namespace OSSIA
{
    class Scenario;
    class TimeProcess;
}

class OSSIAProcessElement : public QObject
{
        Q_OBJECT
    public:
        using QObject::QObject;
        virtual std::shared_ptr<OSSIA::TimeProcess> process() const = 0;
        virtual Process& iscoreProcess() const = 0;
        virtual void stop()
        {
            iscoreProcess().stopExecution();
        }

    signals:
        // Is emitted whenever the implementaton has changed
        // and needs to be put again in the constraint
        void changed(std::shared_ptr<OSSIA::TimeProcess> oldProc,
                     std::shared_ptr<OSSIA::TimeProcess> newProc);
};
