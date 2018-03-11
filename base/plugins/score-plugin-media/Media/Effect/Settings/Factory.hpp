#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <Media/Effect/Settings/Presenter.hpp>
#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/Settings/View.hpp>
namespace Media::Settings
{
SCORE_DECLARE_SETTINGS_FACTORY(
    Factory, Model, Presenter, View, "5cce4a2c-0d64-4400-a9dd-bf5760e46853")
}
