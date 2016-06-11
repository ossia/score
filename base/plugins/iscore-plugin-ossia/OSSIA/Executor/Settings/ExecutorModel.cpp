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

        auto list() {
            return std::tie(Rate);
        }
}

Model::Model(QSettings& set, const iscore::ApplicationContext& ctx)
{
    iscore::setupDefaultSettings(set, Parameters::list(), *this);
}

ISCORE_SETTINGS_PARAMETER_CPP(int, Model, Rate)
}
}
