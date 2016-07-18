#include "LocalTreeModel.hpp"
#include <QSettings>

namespace Ossia
{
namespace LocalTree
{
namespace Settings
{

namespace Parameters
{
        const iscore::sp<ModelLocalTreeParameter> LocalTree{
            QStringLiteral("iscore_plugin_ossia/LocalTree"),
                    false};

        static auto list() {
            return std::tie(LocalTree);
        }
}

Model::Model(QSettings& set, const iscore::ApplicationContext& ctx)
{
    iscore::setupDefaultSettings(set, Parameters::list(), *this);
}

ISCORE_SETTINGS_PARAMETER_CPP(bool, Model, LocalTree)
}
}
}
