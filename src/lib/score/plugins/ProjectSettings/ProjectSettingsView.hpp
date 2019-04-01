#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
namespace score
{
class ProjectSettingsModel;
using ProjectSettingsView = SettingsDelegateView<ProjectSettingsModel>;
// using ProjectSettingsView = SettingsDelegateView<ProjectSettingsModel>;
}
