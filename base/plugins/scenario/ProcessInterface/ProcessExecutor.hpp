#pragma once
#include <ProcessInterface/TimeValue.hpp>

class ProcessExecutor
{
  public:
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void onTick(const TimeValue& time) = 0;
};
