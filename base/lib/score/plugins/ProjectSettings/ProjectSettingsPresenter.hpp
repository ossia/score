#pragma once
#include <QObject>
#include <core/settings/SettingsPresenter.hpp>

namespace score
{
class ProjectSettingsModel;
using ProjectSettingsPresenter
    = SettingsDelegatePresenter<ProjectSettingsModel>;
}
