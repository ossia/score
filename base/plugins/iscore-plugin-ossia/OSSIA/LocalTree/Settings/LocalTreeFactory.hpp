#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <OSSIA/LocalTree/Settings/LocalTreeModel.hpp>
#include <OSSIA/LocalTree/Settings/LocalTreePresenter.hpp>
#include <OSSIA/LocalTree/Settings/LocalTreeView.hpp>

namespace Ossia
{
namespace LocalTree
{
namespace Settings
{
ISCORE_DECLARE_SETTINGS_FACTORY(Factory, Model, Presenter, View, "3cf335f6-8f5d-401b-98a3-eedfd5e7d292")
}
}
}
