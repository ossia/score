#pragma once
#include <iscore_plugin_ossia_export.h>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <Process/TimeValue.hpp>

namespace RecreateOnPlay
{
struct Context;
class BaseScenarioElement;

/**
 * @brief The ClockManager class
 *
 * This class allows to create control mechanisms
 * for the i-score execution clock.
 *
 * The default implementation, DefaultClockManager,
 * uses sleep() and threading to do the execution.
 *
 * Other implementations could be used to synchronize
 * to external clocks, such as the sound card clock,
 * or the host clock if i-score is used as a plug-in.
 *
 * The derived constructors should set-up
 * the clock drive mode, speed, etc.
 *
 */
class ISCORE_PLUGIN_OSSIA_EXPORT ClockManager
{
    public:
        ClockManager(const RecreateOnPlay::Context& ctx): context{ctx} { }
        virtual ~ClockManager();

        const Context& context;

        void play(const TimeValue& t);
        void pause();
        void resume();
        void stop();

    protected:
        virtual void play_impl(
                const TimeValue& t,
                BaseScenarioElement&) = 0;
        virtual void pause_impl(BaseScenarioElement&) = 0;
        virtual void resume_impl(BaseScenarioElement&) = 0;
        virtual void stop_impl(BaseScenarioElement&) = 0;
};

class ISCORE_PLUGIN_OSSIA_EXPORT ClockManagerFactory :
        public iscore::AbstractFactory<ClockManagerFactory>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                ClockManagerFactory,
                "fb2b3624-ee6f-4e9a-901a-a096bb5fec0a")
        public:
            virtual ~ClockManagerFactory();

            virtual QString prettyName() const = 0;
            virtual std::unique_ptr<ClockManager> make(
                const RecreateOnPlay::Context& ctx) = 0;
};

class ISCORE_PLUGIN_OSSIA_EXPORT ClockManagerFactoryList final :
        public iscore::ConcreteFactoryList<ClockManagerFactory>
{
    public:
        using object_type = ClockManager;
};
}

Q_DECLARE_METATYPE(RecreateOnPlay::ClockManagerFactory::ConcreteFactoryKey)
