#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>

#include <QObject>

namespace score
{
class ProjectSettingsModel;
using ProjectSettingsPresenter
    = SettingsDelegatePresenter<ProjectSettingsModel>;
}
