#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <Curve/Settings/CurveSettingsModel.hpp>
#include <Curve/Settings/CurveSettingsPresenter.hpp>
#include <Curve/Settings/CurveSettingsView.hpp>

namespace Curve
{
namespace Settings
{
ISCORE_DECLARE_SETTINGS_FACTORY(
    Factory, Model, Presenter, View, "aa38303e-edbf-4c77-ac4d-5e335df73bca")
}
}
