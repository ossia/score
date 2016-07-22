#include "DefaultClockManager.hpp"

#include <OSSIA/Executor/ExecutorContext.hpp>
#include <OSSIA/Executor/DocumentPlugin.hpp>
#include <OSSIA/Executor/BaseScenarioElement.hpp>
#include <OSSIA/Executor/ConstraintElement.hpp>
#include <OSSIA/Executor/Settings/ExecutorModel.hpp>
#include <OSSIA/OSSIA2iscore.hpp>

#include <Scenario/Document/Constraint/ConstraintDurations.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

#include <Editor/Clock.h>
namespace RecreateOnPlay
{
DefaultClockManager:: ~DefaultClockManager() = default;
DefaultClockManager::DefaultClockManager(
    const Context& ctx) :
    ClockManager{ ctx }
{
    auto& bs = *ctx.sys.baseScenario();
    OSSIA::TimeConstraint& ossia_cst = *bs.baseConstraint()->OSSIAConstraint();

    ossia_cst.setDriveMode(OSSIA::Clock::DriveMode::INTERNAL);
    ossia_cst.setGranularity(context.doc.app.settings<Settings::Model>().getRate());
    ossia_cst.setCallback(makeDefaultCallback(bs));
}
OSSIA::TimeConstraint::ExecutionCallback DefaultClockManager::makeDefaultCallback(
        RecreateOnPlay::BaseScenarioElement& bs)
{
    auto& cst = *bs.baseConstraint();
    return [&bs,&iscore_cst=cst.iscoreConstraint()] (
            OSSIA::TimeValue position,
            OSSIA::TimeValue date,
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
    };
}

void DefaultClockManager::play_impl(
        const TimeValue& t,
        BaseScenarioElement& bs)
{
    bs.baseConstraint()->play(t);
}

void DefaultClockManager::pause_impl(BaseScenarioElement& bs)
{
    bs.baseConstraint()->pause();
}

void DefaultClockManager::resume_impl(BaseScenarioElement& bs)
{
    bs.baseConstraint()->resume();
}

void DefaultClockManager::stop_impl(BaseScenarioElement& bs)
{
    bs.baseConstraint()->stop();
}

DefaultClockManagerFactory::~DefaultClockManagerFactory() = default;
QString DefaultClockManagerFactory::prettyName() const
{
    return QObject::tr("Default");
}

std::unique_ptr<ClockManager> DefaultClockManagerFactory::make(
    const RecreateOnPlay::Context& ctx)
{
    return std::make_unique<DefaultClockManager>( ctx );
}
}
