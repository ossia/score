#pragma once
#include <interface/settingsdelegate/SettingsDelegateModelInterface.hpp>
#include <QString>
#include <QObject>

namespace iscore
{
    class SettingsDelegatePresenterInterface;
}
// TODO find a better way...
#define SETTINGS_CLIENTPORT "NetworkPlugin/ClientPort"
#define SETTINGS_MASTERPORT "NetworkPlugin/MasterPort"
#define SETTINGS_CLIENTNAME "NetworkPlugin/ClientName"

class NetworkSettingsModel : public iscore::SettingsDelegateModelInterface
{
        Q_OBJECT
    public:
        NetworkSettingsModel();

        void setClientName(QString txt);
        QString getClientName() const;
        void setClientPort(int val);
        int getClientPort() const;
        void setMasterPort(int val);
        int getMasterPort() const;

        virtual void setPresenter(iscore::SettingsDelegatePresenterInterface* presenter) ;  // @todo remove this and the same for view
        virtual void setFirstTimeSettings() override;

    signals:
        void clientNameChanged();
        void clientPortChanged();
        void masterPortChanged();

    private:
        int masterPort;
        int clientPort;
        QString clientName;
};
