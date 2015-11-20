#pragma once
#include <Process/Process.hpp>
#include <ProcessModel/TimeProcessWithConstraint.hpp>
#include <memory>

namespace RecreateOnPlay
{

class OSSIAProcessModel : public Process
{
    public:
        using Process::Process;
        virtual std::shared_ptr<TimeProcessWithConstraint> process() const = 0;
};
}
