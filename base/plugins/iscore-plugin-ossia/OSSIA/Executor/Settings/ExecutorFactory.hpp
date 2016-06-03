#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <OSSIA/Executor/Settings/ExecutorModel.hpp>
#include <OSSIA/Executor/Settings/ExecutorPresenter.hpp>
#include <OSSIA/Executor/Settings/ExecutorView.hpp>

namespace RecreateOnPlay
{
namespace Settings
{
ISCORE_DECLARE_SETTINGS_FACTORY(Factory, Model, Presenter, View, "f418e1a0-fdff-45ec-99b2-b208706badc8")
}
}
