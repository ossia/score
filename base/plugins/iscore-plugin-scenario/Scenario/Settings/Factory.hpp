#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactoryInterface.hpp>

#include <Scenario/Settings/Model.hpp>
#include <Scenario/Settings/Presenter.hpp>
#include <Scenario/Settings/View.hpp>

namespace Scenario
{
namespace Settings
{
ISCORE_DECLARE_SETTINGS_FACTORY(Factory, Model, Presenter, View, "27ab096d-b9df-4ca9-9442-1ebd697a8fab")
}
}
