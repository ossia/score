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


namespace Explorer::ProjectSettings
{
Model::Model(const score::DocumentContext& ctx,
             Id<DocumentPlugin> id,
             QObject* parent):
    ProjectSettingsModel{ctx, id, "ExplorerSettings", parent}
{
}

SCORE_PROJECTSETTINGS_PARAMETER_CPP(bool, Model, RefreshOnStart)
SCORE_PROJECTSETTINGS_PARAMETER_CPP(bool, Model, ReconnectOnStart)
}

template <>
void DataStreamReader::read(const Explorer::ProjectSettings::Model& model)
{
  m_stream << model.m_RefreshOnStart << model.m_ReconnectOnStart;
}

template <>
void DataStreamWriter::write(Explorer::ProjectSettings::Model& model)
{
  m_stream >> model.m_RefreshOnStart >> model.m_ReconnectOnStart;
}

template <>
void JSONObjectReader::read(const Explorer::ProjectSettings::Model& model)
{
  obj["Refresh"] = model.m_RefreshOnStart;
  obj["Reconnect"] = model.m_ReconnectOnStart;
}

template <>
void JSONObjectWriter::write(Explorer::ProjectSettings::Model& model)
{
  model.m_RefreshOnStart = obj["Refresh"].toBool();
  model.m_ReconnectOnStart = obj["Reconnect"].toBool();
}
