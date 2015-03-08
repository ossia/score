#pragma once
#include <plugin_interface/settingsdelegate/SettingsDelegatePresenterInterface.hpp>
#include <QObject>

class PluginSettingsModel;
class PluginSettingsView;
class BlacklistCommand;
class PluginSettingsPresenter : public iscore::SettingsDelegatePresenterInterface
{
        Q_OBJECT
    public:
        PluginSettingsPresenter(iscore::SettingsPresenter* parent,
                                iscore::SettingsDelegateModelInterface* model,
                                iscore::SettingsDelegateViewInterface* view);

        virtual void on_accept() override;
        virtual void on_reject() override;

        virtual QString settingsName() override
        {
            return tr("Plugin");
        }

        virtual QIcon settingsIcon() override;

        void load();
        PluginSettingsModel* model();
        PluginSettingsView* view();

    public slots:
        void setBlacklistCommand(BlacklistCommand* cmd);

    private:

        // S'il y avait plusieurs contrôles chaque contrôle devrait avoir sa "commande".
        iscore::Command* m_blacklistCommand {nullptr};
};
