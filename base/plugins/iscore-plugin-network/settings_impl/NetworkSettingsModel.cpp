#include <qsettings.h>
#include <qvariant.h>

#include "NetworkSettingsModel.hpp"
#include "iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp"

using namespace iscore;

NetworkSettingsModel::NetworkSettingsModel() :
    iscore::SettingsDelegateModelInterface {}
{
    this->setObjectName("NetworkSettingsModel");

    QSettings s;

    if(!s.contains(SETTINGS_CLIENTPORT))
    {
        setFirstTimeSettings();
    }

    setClientPort(s.value(SETTINGS_CLIENTPORT).toInt());
    setMasterPort(s.value(SETTINGS_MASTERPORT).toInt());
    setClientName(s.value(SETTINGS_CLIENTNAME).toString());
}

void NetworkSettingsModel::setClientName(QString txt)
{
    clientName = txt;
    QSettings s;
    s.setValue(SETTINGS_CLIENTNAME, txt);
    emit clientNameChanged();
}

QString NetworkSettingsModel::getClientName() const
{
    return clientName;
}

void NetworkSettingsModel::setClientPort(int val)
{
    clientPort = val;
    QSettings s;
    s.setValue(SETTINGS_CLIENTPORT, val);
    emit clientPortChanged();
}

int NetworkSettingsModel::getClientPort() const
{
    return clientPort;
}

void NetworkSettingsModel::setMasterPort(int val)
{
    masterPort = val;
    QSettings s;
    s.setValue(SETTINGS_MASTERPORT, val);
    emit masterPortChanged();
}

int NetworkSettingsModel::getMasterPort() const
{
    return masterPort;
}

void NetworkSettingsModel::setPresenter(SettingsDelegatePresenterInterface* presenter)
{
}

void NetworkSettingsModel::setFirstTimeSettings()
{
    QSettings s;
    s.setValue(SETTINGS_CLIENTNAME, "i-score client");
    s.setValue(SETTINGS_CLIENTPORT, 7888);
    s.setValue(SETTINGS_MASTERPORT, 5678);
}
