#pragma once
#include <Protocols/Settings/Model.hpp>
#include <Protocols/Settings/Presenter.hpp>
#include <Protocols/Settings/View.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
namespace Protocols::Settings
{
SCORE_DECLARE_SETTINGS_FACTORY(
    Factory, Model, Presenter, View, "372d4801-8170-451f-9db1-346652290e5c")
}
