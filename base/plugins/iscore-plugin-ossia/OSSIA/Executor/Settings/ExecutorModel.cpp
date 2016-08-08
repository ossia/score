#include "ExecutorModel.hpp"
#include <OSSIA/Executor/ClockManager/DefaultClockManager.hpp>
namespace Engine { namespace Execution
{
namespace Settings
{

namespace Parameters
{
        const iscore::sp<ModelRateParameter> Rate{
            QStringLiteral("iscore_plugin_ossia/ExecutionRate"),
                    50};
        const iscore::sp<ModelClockParameter> Clock{
            QStringLiteral("iscore_plugin_ossia/Clock"),
                    DefaultClockManagerFactory::static_concreteFactoryKey()};

        static auto list() {
            return std::tie(Rate, Clock);
        }
}

Model::Model(QSettings& set, const iscore::ApplicationContext& ctx):
    m_clockFactories{ctx.components.factory<ClockManagerFactoryList>()}
{
    iscore::setupDefaultSettings(set, Parameters::list(), *this);
}

std::unique_ptr<ClockManager> Model::makeClock(
        const Engine::Execution::Context& ctx) const
{
    auto it = m_clockFactories.find(m_Clock);
    return it != m_clockFactories.end()
                 ? it->make(ctx)
                 : std::make_unique<DefaultClockManager>( ctx );
}

ISCORE_SETTINGS_PARAMETER_CPP(int, Model, Rate)
ISCORE_SETTINGS_PARAMETER_CPP(ClockManagerFactory::ConcreteFactoryKey, Model, Clock)
}
} }
