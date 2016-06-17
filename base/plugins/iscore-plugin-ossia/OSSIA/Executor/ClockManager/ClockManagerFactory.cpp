#include "ClockManagerFactory.hpp"
#include <OSSIA/Executor/ExecutorContext.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
namespace RecreateOnPlay
{

ClockManager::~ClockManager() = default;
ClockManagerFactory::~ClockManagerFactory() = default;

void ClockManager::play(const TimeValue& t)
{
    auto bs = context.sys.baseScenario();
    if(!bs)
        return;

    play_impl(t, *bs);
}

void ClockManager::pause()
{
    auto bs = context.sys.baseScenario();
    if(!bs)
        return;

    pause_impl(*bs);
}

void ClockManager::resume()
{
    auto bs = context.sys.baseScenario();
    if(!bs)
        return;

    resume_impl(*bs);
}

void ClockManager::stop()
{
    auto bs = context.sys.baseScenario();
    if(!bs)
        return;

    stop_impl(*bs);
}

}
