#include "ExecutorModel.hpp"
namespace RecreateOnPlay
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
                    };

        auto list() {
            return std::tie(Rate, Clock);
        }
}

Model::Model(QSettings& set, const iscore::ApplicationContext& ctx):
    m_clockFactories{ctx.components.factory<ClockManagerFactoryList>()}
{
    iscore::setupDefaultSettings(set, Parameters::list(), *this);
}

std::unique_ptr<ClockManager> Model::makeClock(const RecreateOnPlay::Context& ctx) const
{
    // 1. Create an uuid from the QString.
    auto txt = m_Clock.toLatin1();
    auto uid = ClockManagerFactory::ConcreteFactoryKey{txt.constData()};

    auto it = m_clockFactories.find(std::move(uid));
    return it != m_clockFactories.end()
                ? it->make(ctx)
                : nullptr;
}

ISCORE_SETTINGS_PARAMETER_CPP(int, Model, Rate)
ISCORE_SETTINGS_PARAMETER_CPP(QString, Model, Clock)
}
}
