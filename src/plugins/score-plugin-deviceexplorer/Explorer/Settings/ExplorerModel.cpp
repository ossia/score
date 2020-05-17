// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ExplorerModel.hpp"

#include <QSettings>

#include <wobjectimpl.h>

namespace Explorer::Settings
{
namespace Parameters
{
SETTINGS_PARAMETER_IMPL(LocalTree){QStringLiteral("score_plugin_LocalTree"), true};
SETTINGS_PARAMETER_IMPL(LogLevel){
    QStringLiteral("score_plugin_engine/LogLevel"),
    DeviceLogLevel{}.logEverything};

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
Model::Model(const score::DocumentContext& ctx, Id<DocumentPlugin> id, QObject* parent)
    : ProjectSettingsModel{ctx, id, "ExplorerSettings", parent}
{
}
Model::~Model() { }

SCORE_PROJECTSETTINGS_PARAMETER_CPP(qreal, Model, MidiImportRatio)
SCORE_PROJECTSETTINGS_PARAMETER_CPP(bool, Model, RefreshOnStart)
SCORE_PROJECTSETTINGS_PARAMETER_CPP(bool, Model, ReconnectOnStart)
}

template <>
void DataStreamReader::read(const Explorer::ProjectSettings::Model& model)
{
  m_stream << model.m_MidiImportRatio << model.m_RefreshOnStart << model.m_ReconnectOnStart;
}

template <>
void DataStreamWriter::write(Explorer::ProjectSettings::Model& model)
{
  m_stream >> model.m_MidiImportRatio >> model.m_RefreshOnStart >> model.m_ReconnectOnStart;
}

template <>
void JSONReader::read(const Explorer::ProjectSettings::Model& model)
{
  obj["Refresh"] = model.m_RefreshOnStart;
  obj["Reconnect"] = model.m_ReconnectOnStart;
  obj["MidiRatio"] = model.m_MidiImportRatio;
}

template <>
void JSONWriter::write(Explorer::ProjectSettings::Model& model)
{
  model.m_RefreshOnStart = obj["Refresh"].toBool();
  model.m_ReconnectOnStart = obj["Reconnect"].toBool();
  model.m_MidiImportRatio = obj["MidiRatio"].toDouble();
}
