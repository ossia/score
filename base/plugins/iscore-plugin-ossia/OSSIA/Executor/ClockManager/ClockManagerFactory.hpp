#pragma once
#include <iscore_plugin_ossia_export.h>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace RecreateOnPlay
{
class BaseScenarioRefContainer;

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
 * TODO use this to clean-up the requestPlay, requestStop, etc... mess.
 *
 */
class ISCORE_PLUGIN_OSSIA_EXPORT ClockManager
{
    public:
        virtual ~ClockManager();

        virtual void play() = 0;
        virtual void pause() = 0;
        virtual void stop() = 0;

        virtual void setup(BaseScenarioRefContainer&) = 0;
};

class ISCORE_PLUGIN_OSSIA_EXPORT ClockManagerFactory :
        public iscore::AbstractFactory<ClockManagerFactory>
{
        ISCORE_ABSTRACT_FACTORY_DECL(
                ClockManagerFactory,
                "fb2b3624-ee6f-4e9a-901a-a096bb5fec0a")
        public:
            virtual ~ClockManagerFactory();

            virtual std::unique_ptr<ClockManager> make(
                const iscore::ApplicationContext& ctx) = 0;
};

class ISCORE_LIB_BASE_EXPORT ClockManagerFactoryList final :
        public iscore::ConcreteFactoryList<ClockManagerFactory>
{
    public:
        using object_type = ClockManager;
};
}
