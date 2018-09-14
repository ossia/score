#pragma once
#include <PluginSettings/PluginSettingsModel.hpp>
#include <PluginSettings/PluginSettingsPresenter.hpp>
#include <PluginSettings/PluginSettingsView.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

namespace PluginSettings
{
SCORE_DECLARE_SETTINGS_FACTORY(
    Factory, PluginSettingsModel, PluginSettingsPresenter, PluginSettingsView,
    "96d1d318-76d3-4313-87d8-0e80ba18ac28")
}
