#pragma once
#include <memory>
namespace Process { class ProcessModel; }
namespace RecreateOnPlay
{
class ProcessComponent;

struct ProcessWrapper
{
        virtual ~ProcessWrapper();
};

struct OSSIAProcess
{
        OSSIAProcess() = default;
        OSSIAProcess(OSSIAProcess&&) = default;
        OSSIAProcess& operator=(OSSIAProcess&&) = default;

        OSSIAProcess(ProcessComponent* e, std::unique_ptr<ProcessWrapper>&& proc):
            element(e),
            wrapper(std::move(proc))
        {

        }

        ProcessComponent* element{};
        std::unique_ptr<ProcessWrapper> wrapper;
};
}
