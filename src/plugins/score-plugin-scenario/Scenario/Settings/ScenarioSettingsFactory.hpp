#pragma once
#include <Scenario/Settings/ScenarioSettingsModel.hpp>
#include <Scenario/Settings/ScenarioSettingsPresenter.hpp>
#include <Scenario/Settings/ScenarioSettingsView.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

namespace Scenario
{
namespace Settings
{
SCORE_DECLARE_SETTINGS_FACTORY(
    Factory,
    Model,
    Presenter,
    View,
    "27ab096d-b9df-4ca9-9442-1ebd697a8fab")
}
}
