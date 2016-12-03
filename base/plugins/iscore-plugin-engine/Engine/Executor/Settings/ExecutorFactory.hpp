#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <Engine/Executor/Settings/ExecutorPresenter.hpp>
#include <Engine/Executor/Settings/ExecutorView.hpp>

namespace Engine
{
namespace Execution
{
namespace Settings
{
ISCORE_DECLARE_SETTINGS_FACTORY(
    Factory, Model, Presenter, View, "f418e1a0-fdff-45ec-99b2-b208706badc8")
}
}
}
