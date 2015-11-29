#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegatePresenterInterface.hpp>
#include <qicon.h>
#include <qobjectdefs.h>
#include <qstring.h>

class ClientNameChangedCommand;
class ClientPortChangedCommand;
class MasterPortChangedCommand;
class NetworkSettingsModel;
class NetworkSettingsView;
namespace iscore {
class Command;
class SettingsDelegateModelInterface;
class SettingsDelegateViewInterface;
class SettingsPresenter;
}  // namespace iscore

class NetworkSettingsPresenter : public iscore::SettingsDelegatePresenterInterface
{
        Q_OBJECT
    public:
        NetworkSettingsPresenter(iscore::SettingsPresenter* parent,
                                 iscore::SettingsDelegateModelInterface* model,
                                 iscore::SettingsDelegateViewInterface* view);

        void on_accept() override;
        void on_reject() override;

        QString settingsName() override
        {
            return tr("Network");
        }

        QIcon settingsIcon() override;

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
