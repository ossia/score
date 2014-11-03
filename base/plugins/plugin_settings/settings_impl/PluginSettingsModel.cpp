#include "PluginSettingsModel.hpp"
#include <QDebug>
#include <QSettings>

using namespace iscore;

PluginSettingsModel::PluginSettingsModel():
	iscore::SettingsGroupModel{}
{
	this->setObjectName("PluginSettingsModel");

	QSettings s;
/*	if(!s.contains(SETTINGS_CLIENTPORT))
		setFirstTimeSettings();
*/
	m_plugins = new QStandardItemModel(1, 1, this);
	QStandardItem *item = new QStandardItem(QString("Plop"));
	item->setCheckable(true);
	m_plugins->setItem(0, 0, item);
/*
	setClientPort(s.value(SETTINGS_CLIENTPORT).toInt());
	setMasterPort(s.value(SETTINGS_MASTERPORT).toInt());
	setClientName(s.value(SETTINGS_CLIENTNAME).toString());
*/}
/*
void PluginSettingsModel::setClientName(QString txt)
{
	clientName = txt;
	QSettings s;
	s.setValue(SETTINGS_CLIENTNAME, txt);
	emit clientNameChanged();
}

QString PluginSettingsModel::getClientName() const
{
	return clientName;
}

void PluginSettingsModel::setClientPort(int val)
{
	clientPort = val;
	QSettings s;
	s.setValue(SETTINGS_CLIENTPORT, val);
	emit clientPortChanged();
}

int PluginSettingsModel::getClientPort() const
{
	return clientPort;
}

void PluginSettingsModel::setMasterPort(int val)
{
	masterPort = val;
	QSettings s;
	s.setValue(SETTINGS_MASTERPORT, val);
	emit masterPortChanged();
}

int PluginSettingsModel::getMasterPort() const
{
	return masterPort;
}
*/
void PluginSettingsModel::setPresenter(SettingsGroupPresenter* presenter)
{
}

void PluginSettingsModel::setFirstTimeSettings()
{
	QSettings s;
	/*s.setValue(SETTINGS_CLIENTNAME, "i-score client");
	s.setValue(SETTINGS_CLIENTPORT, 7888);
	s.setValue(SETTINGS_MASTERPORT, 5678);*/
}
