#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <PluginSettings/PluginSettingsModel.hpp>
#include <PluginSettings/PluginSettingsPresenter.hpp>
#include <PluginSettings/PluginSettingsView.hpp>

namespace PluginSettings
{
ISCORE_DECLARE_SETTINGS_FACTORY(Factory, PluginSettingsModel, PluginSettingsPresenter, PluginSettingsView, "96d1d318-76d3-4313-87d8-0e80ba18ac28")
}
