#pragma once
#include "LoopingProcessWrapper.hpp"
class Process;
class OSSIAProcessElement;


using ProcessWrapper = LoopingProcessWrapper;

struct OSSIAProcess
{
        OSSIAProcess() = default;
        OSSIAProcess(OSSIAProcess&&) = default;
        OSSIAProcess& operator=(OSSIAProcess&&) = default;

        OSSIAProcess(OSSIAProcessElement* e, std::unique_ptr<ProcessWrapper>&& proc):
            element(e),
            wrapper(std::move(proc))
        {

        }

        OSSIAProcessElement* element{};
        std::unique_ptr<ProcessWrapper> wrapper;
};
