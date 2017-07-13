// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LocalTreeModel.hpp"
#include <QSettings>

namespace Engine
{
namespace LocalTree
{
namespace Settings
{

namespace Parameters
{
const iscore::sp<ModelLocalTreeParameter> LocalTree{
    QStringLiteral("iscore_plugin_engine/LocalTree"), true};

static auto list()
{
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
