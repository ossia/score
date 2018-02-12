#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <Explorer/Settings/ExplorerModel.hpp>
#include <Explorer/Settings/ExplorerPresenter.hpp>
#include <Explorer/Settings/ExplorerView.hpp>

namespace Explorer::Settings
{
SCORE_DECLARE_SETTINGS_FACTORY(
    Factory, Model, Presenter, View, "3cf335f6-8f5d-401b-98a3-eedfd5e7d292")
}
