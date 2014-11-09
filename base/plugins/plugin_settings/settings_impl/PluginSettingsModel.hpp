#pragma once
#include <interface/settingsdelegate/SettingsDelegateModelInterface.hpp>
#include <QStandardItemModel>
#include <QString>
#include <QObject>

namespace iscore
{
	class SettingsDelegatePresenterInterface;
}
// TODO find a better way...
/*#define SETTINGS_CLIENTPORT "PluginPlugin/ClientPort"
#define SETTINGS_MASTERPORT "PluginPlugin/MasterPort"
#define SETTINGS_CLIENTNAME "PluginPlugin/ClientName"
*/

class PluginSettingsPresenter;
class PluginSettingsModel : public iscore::SettingsDelegateModelInterface
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
		virtual void setPresenter(iscore::SettingsDelegatePresenterInterface* presenter) override;
		virtual void setFirstTimeSettings() override;
		PluginSettingsPresenter* presenter() { return m_presenter; }

	public slots:
		void on_itemChanged(QStandardItem*);

	private:
		QStandardItemModel* m_plugins{};
		PluginSettingsPresenter* m_presenter;
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
