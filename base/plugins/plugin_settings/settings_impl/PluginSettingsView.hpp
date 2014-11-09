#pragma once
#include <interface/settingsdelegate/SettingsDelegateViewInterface.hpp>
#include <core/presenter/command/Command.hpp>
#include <QListView>

class PluginSettingsPresenter;
class PluginSettingsView : public iscore::SettingsDelegateViewInterface
{
		Q_OBJECT
	public:
		PluginSettingsView(QObject* parent);

		QListView* view() { return m_listView; }
/*
		void setMasterPort(int val);
		void setClientPort(int val);
		void setClientName(QString text);
*/
		virtual QWidget* getWidget() override;
		void load();
		void doConnections();

	signals:
		void submitCommand(iscore::Command* cmd);

	public slots:
/*		void on_masterPortChanged(int);
		void on_clientPortChanged(int);
		void on_clientNameChanged();
*/
	private:
		PluginSettingsPresenter* presenter();

		QWidget* m_widget{new QWidget};
		QListView* m_listView{new QListView{m_widget}};
/*
		QSpinBox* m_masterPort{new QSpinBox(this)};
		QSpinBox* m_clientPort{new QSpinBox(this)};
		QLineEdit* m_clientName{new QLineEdit(this)};

		int m_previousMasterPort{};
		int m_previousClientPort{};
		QString m_previousClientName{};
*/};
