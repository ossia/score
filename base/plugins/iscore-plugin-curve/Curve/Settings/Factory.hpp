#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <Curve/Settings/Model.hpp>
#include <Curve/Settings/Presenter.hpp>
#include <Curve/Settings/View.hpp>

namespace Curve
{
namespace Settings
{
ISCORE_DECLARE_SETTINGS_FACTORY(Factory, Model, Presenter, View, "aa38303e-edbf-4c77-ac4d-5e335df73bca")
}
}
