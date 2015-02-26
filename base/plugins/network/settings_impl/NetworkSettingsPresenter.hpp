#pragma once
#include <interface/settingsdelegate/SettingsDelegatePresenterInterface.hpp>
#include <QObject>

class NetworkSettingsModel;
class NetworkSettingsView;
class MasterPortChangedCommand;
class ClientPortChangedCommand;
class ClientNameChangedCommand;
class NetworkSettingsPresenter : public iscore::SettingsDelegatePresenterInterface
{
        Q_OBJECT
    public:
        NetworkSettingsPresenter(iscore::SettingsPresenter* parent,
                                 iscore::SettingsDelegateModelInterface* model,
                                 iscore::SettingsDelegateViewInterface* view);

        virtual void on_accept() override;
        virtual void on_reject() override;

        virtual QString settingsName() override
        {
            return tr("Network");
        }

        virtual QIcon settingsIcon() override;

        void load();
        NetworkSettingsModel* model();
        NetworkSettingsView* view();

    public slots:
        void updateMasterPort();
        void updateClientPort();
        void updateClientName();

        void setMasterPortCommand(MasterPortChangedCommand* cmd);
        void setClientPortCommand(ClientPortChangedCommand* cmd);
        void setClientNameCommand(ClientNameChangedCommand* cmd);

    private:
        // S'il y avait plusieurs contrôles chaque contrôle devrait avoir sa "commande".
        iscore::Command* m_masterportCommand {nullptr};
        iscore::Command* m_clientportCommand {nullptr};
        iscore::Command* m_clientnameCommand {nullptr};
};
