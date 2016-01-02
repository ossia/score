#pragma once
#include <Process/Process.hpp>
#include <OSSIA/ProcessModel/TimeProcessWithConstraint.hpp>
#include <memory>

#include <Process/StateProcess.hpp>
namespace RecreateOnPlay
{

class OSSIAProcessModel : public Process::ProcessModel
{
    public:
        using Process::ProcessModel::ProcessModel;
        virtual std::shared_ptr<TimeProcessWithConstraint> process() const = 0;
};


class OSSIAStateProcessModel : public Process::StateProcess
{
    public:
        using Process::StateProcess::StateProcess;
        virtual std::shared_ptr<OSSIA::StateElement> state() const = 0;
};

}
