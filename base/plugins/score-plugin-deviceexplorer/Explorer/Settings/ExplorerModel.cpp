// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExplorerModel.hpp"
#include <QSettings>

namespace Explorer::Settings
{
namespace Parameters
{
const score::sp<ModelLocalTreeParameter> LocalTree{
    QStringLiteral("score_plugin_engine/LocalTree"), true};
const score::sp<ModelLogLevelParameter> LogLevel{
  QStringLiteral("score_plugin_engine/LogLevel"), DeviceLogLevel{}.logEverything};

static auto list()
{
  return std::tie(LocalTree, LogLevel);
}
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

SCORE_SETTINGS_PARAMETER_CPP(bool, Model, LocalTree)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, LogLevel)
}
