#pragma once
#include "BasicProcessWrapper.hpp"
#include "LoopingProcessWrapper.hpp"
class Process;


namespace RecreateOnPlay
{
class ProcessElement;
using ProcessWrapper = LoopingProcessWrapper;

struct OSSIAProcess
{
        OSSIAProcess() = default;
        OSSIAProcess(OSSIAProcess&&) = default;
        OSSIAProcess& operator=(OSSIAProcess&&) = default;

        OSSIAProcess(ProcessElement* e, std::unique_ptr<ProcessWrapper>&& proc):
            element(e),
            wrapper(std::move(proc))
        {

        }

        ProcessElement* element{};
        std::unique_ptr<ProcessWrapper> wrapper;
};
}
