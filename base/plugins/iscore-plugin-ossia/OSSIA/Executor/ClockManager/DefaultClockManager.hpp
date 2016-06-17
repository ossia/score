#pragma once
#include <OSSIA/Executor/ClockManager/ClockManagerFactory.hpp>

namespace RecreateOnPlay
{
class DefaultClockManager final : public ClockManager
{
    public:
        DefaultClockManager(const Context& ctx);

    private:
        void play_impl(BaseScenarioElement&) override;
        void pause_impl(BaseScenarioElement&) override;
        void resume_impl(BaseScenarioElement&) override;
        void stop_impl(BaseScenarioElement&) override;
        void setup_impl(BaseScenarioElement&) override;
};
class DefaultClockManagerFactory final : public ClockManagerFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("583e9c52-e136-46b6-852f-7eef2993e9eb")

        virtual std::unique_ptr<ClockManager> make(
            const RecreateOnPlay::Context& ctx) override;
};

}
