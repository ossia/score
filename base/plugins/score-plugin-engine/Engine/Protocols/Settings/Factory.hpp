#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <Engine/Protocols/Settings/Presenter.hpp>
#include <Engine/Protocols/Settings/Model.hpp>
#include <Engine/Protocols/Settings/View.hpp>
namespace Audio::Settings
{
SCORE_DECLARE_SETTINGS_FACTORY(
    Factory, Model, Presenter, View, "a6456074-da40-4751-b0ec-a447d4952c9d")
}
