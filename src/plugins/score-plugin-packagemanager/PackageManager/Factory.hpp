#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <PackageManager/Model.hpp>
#include <PackageManager/Presenter.hpp>
#include <PackageManager/View.hpp>

namespace PM
{
SCORE_DECLARE_SETTINGS_FACTORY(
    Factory, PluginSettingsModel, PluginSettingsPresenter, PluginSettingsView,
    "96d1d318-76d3-4313-87d8-0e80ba18ac28")
}
