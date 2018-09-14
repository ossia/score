#pragma once
#include <Curve/Settings/CurveSettingsModel.hpp>
#include <Curve/Settings/CurveSettingsPresenter.hpp>
#include <Curve/Settings/CurveSettingsView.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

namespace Curve
{
namespace Settings
{
SCORE_DECLARE_SETTINGS_FACTORY(
    Factory, Model, Presenter, View, "aa38303e-edbf-4c77-ac4d-5e335df73bca")
}
}
