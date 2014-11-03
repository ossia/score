#pragma once
#include <interface/settings/SettingsGroupModel.hpp>
#include <QStandardItemModel>
#include <QString>
#include <QObject>

namespace iscore
{
	class SettingsGroupPresenter;
}
// TODO find a better way...
/*#define SETTINGS_CLIENTPORT "PluginPlugin/ClientPort"
#define SETTINGS_MASTERPORT "PluginPlugin/MasterPort"
#define SETTINGS_CLIENTNAME "PluginPlugin/ClientName"
*/
class PluginSettingsModel : public iscore::SettingsGroupModel
{
		Q_OBJECT
	public:
		PluginSettingsModel();
		QStandardItemModel* model() { return m_plugins; }
/*
		void setClientName(QString txt);
		QString getClientName() const;
		void setClientPort(int val);
		int getClientPort() const;
		void setMasterPort(int val);
		int getMasterPort() const;
*/
		virtual void setPresenter(iscore::SettingsGroupPresenter* presenter) override;
		virtual void setFirstTimeSettings() override;

	private:
		QStandardItemModel* m_plugins{};
/*
	signals:
		void clientNameChanged();
		void clientPortChanged();
		void masterPortChanged();

	private:
		int masterPort;
		int clientPort;
		QString clientName;
*/};
