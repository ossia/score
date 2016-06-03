#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>
#include <QIcon>

#include <QString>

namespace iscore {
class Command;
class SettingsDelegateModel;
class SettingsDelegateView;
class SettingsPresenter;
}  // namespace iscore

namespace PluginSettings
{
class BlacklistCommand;
class PluginSettingsModel;
class PluginSettingsView;
class PluginSettingsPresenter : public iscore::SettingsDelegatePresenter
{
        Q_OBJECT
    public:
        PluginSettingsPresenter(
                iscore::SettingsDelegateModel& model,
                iscore::SettingsDelegateView& view,
                QObject* parent);

    private:
        QString settingsName() override
        {
            return tr("Plugin");
        }

        QIcon settingsIcon() override;


};
}
