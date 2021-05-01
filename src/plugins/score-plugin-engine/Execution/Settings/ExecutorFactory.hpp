#pragma once
#include <Execution/Settings/ExecutorModel.hpp>
#include <Execution/Settings/ExecutorPresenter.hpp>
#include <Execution/Settings/ExecutorView.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

namespace Execution
{
namespace Settings
{
SCORE_DECLARE_SETTINGS_FACTORY(
    Factory,
    Model,
    Presenter,
    View,
    "f418e1a0-fdff-45ec-99b2-b208706badc8")
}
}
