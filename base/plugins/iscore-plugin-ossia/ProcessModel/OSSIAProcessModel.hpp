#pragma once
#include <ProcessInterface/Process.hpp>
#include <memory>

namespace OSSIA
{
class TimeProcess;
}

class OSSIAProcessModel : public Process
{
    public:
        virtual std::shared_ptr<OSSIA::TimeProcess> process() const = 0;
};
