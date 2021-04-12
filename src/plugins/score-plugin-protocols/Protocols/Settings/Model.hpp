#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsModel.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <score_plugin_protocols_export.h>

#include <verdigris>
#include <libremidi/api.hpp>

namespace Protocols::Settings
{
struct MidiAPI
{
  operator QStringList() const;
};

class SCORE_PLUGIN_PROTOCOLS_EXPORT Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)

  QString m_MidiAPI;

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  libremidi::API getMidiApiAsEnum() const noexcept;
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_PROTOCOLS_EXPORT, QString, MidiAPI)
};

SCORE_SETTINGS_PARAMETER(Model, MidiAPI)
}

#undef AUDIO_PARAMETER_HPP
