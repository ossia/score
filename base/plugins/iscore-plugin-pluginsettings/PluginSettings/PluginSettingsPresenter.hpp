#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegatePresenterInterface.hpp>
#include <QIcon>

#include <QString>

namespace iscore {
class Command;
class SettingsDelegateModelInterface;
class SettingsDelegateViewInterface;
class SettingsPresenter;
}  // namespace iscore

namespace PluginSettings
{
class BlacklistCommand;
class PluginSettingsModel;
class PluginSettingsView;
class PluginSettingsPresenter : public iscore::SettingsDelegatePresenterInterface
{
        Q_OBJECT
    public:
        PluginSettingsPresenter(
                iscore::SettingsDelegateModelInterface& model,
                iscore::SettingsDelegateViewInterface& view,
                QObject* parent);

    private:
        QString settingsName() override
        {
            return tr("Plugin");
        }

        QIcon settingsIcon() override;


};
}
