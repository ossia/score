#pragma once
#include <core/settings/SettingsPresenter.hpp>

#include <QObject>

namespace score
{
class ProjectSettingsModel;
using ProjectSettingsPresenter
    = SettingsDelegatePresenter<ProjectSettingsModel>;
}
