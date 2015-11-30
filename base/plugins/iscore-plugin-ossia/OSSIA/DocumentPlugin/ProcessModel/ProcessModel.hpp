#pragma once
#include <Process/Process.hpp>
#include <OSSIA/ProcessModel/TimeProcessWithConstraint.hpp>
#include <memory>

#include <Process/StateProcess.hpp>
namespace RecreateOnPlay
{

class OSSIAProcessModel : public Process
{
    public:
        using Process::Process;
        virtual std::shared_ptr<TimeProcessWithConstraint> process() const = 0;
};


class OSSIAStateProcessModel : public StateProcess
{
    public:
        using StateProcess::StateProcess;
        virtual std::shared_ptr<OSSIA::StateElement> state() const = 0;
};

}
