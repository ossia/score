#pragma once
#include <Gfx/Settings/Model.hpp>
#include <Gfx/Settings/Presenter.hpp>
#include <Gfx/Settings/View.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
namespace Gfx::Settings
{
SCORE_DECLARE_SETTINGS_FACTORY(
    Factory,
    Model,
    Presenter,
    View,
    "90bc7d6e-94ca-4acc-8f1c-db76185e86f8")
}
