#include "DefaultClockManager.hpp"
#include <OSSIA/Executor/ExecutorContext.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <OSSIA/Executor/BaseScenarioElement.hpp>
#include <OSSIA/Executor/ConstraintElement.hpp>
#include <OSSIA/Executor/Settings/ExecutorModel.hpp>
#include <OSSIA/OSSIA2iscore.hpp>

#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <Editor/TimeConstraint.h>
#include <Editor/Clock.h>
namespace RecreateOnPlay
{

DefaultClockManager::DefaultClockManager(
        const Context& ctx):
    ClockManager{ctx}
{

}

void DefaultClockManager::play_impl(BaseScenarioElement&)
{

}

void DefaultClockManager::pause_impl(BaseScenarioElement&)
{
}
void DefaultClockManager::resume_impl(BaseScenarioElement&)
{
}

void DefaultClockManager::stop_impl(BaseScenarioElement&)
{
}

void DefaultClockManager::setup_impl(BaseScenarioElement& bs)
{
    OSSIA::TimeConstraint& ossia_cst = *bs.baseConstraint()->OSSIAConstraint();
    ossia_cst.setDriveMode(OSSIA::Clock::DriveMode::INTERNAL);
    ossia_cst.setGranularity(context.doc.app.settings<Settings::Model>().getRate());
    ossia_cst.setCallback([&,&iscore_cst=bs.baseConstraint()->iscoreConstraint()] (const OSSIA::TimeValue& position,
                          const OSSIA::TimeValue& date,
                          std::shared_ptr<OSSIA::StateElement> state)
{
        state->launch();

        auto currentTime = Ossia::convert::time(date);

        auto& cstdur = iscore_cst.duration;
        const auto& maxdur = cstdur.maxDuration();

        if(!maxdur.isInfinite())
            cstdur.setPlayPercentage(currentTime / cstdur.maxDuration());
        else
            cstdur.setPlayPercentage(currentTime / cstdur.defaultDuration());
});
}

std::unique_ptr<ClockManager> DefaultClockManagerFactory::make(
        const RecreateOnPlay::Context& ctx)
{
    return std::make_unique<DefaultClockManager>(ctx);
}

}
