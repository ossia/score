#pragma once
#include <interface/settings/SettingsGroup.hpp>
#include <core/presenter/command/Command.hpp>
#include <QListView>

class PluginSettingsPresenter;
class PluginSettingsView : public QWidget, public iscore::SettingsGroupView
{
		Q_OBJECT
	public:
		PluginSettingsView(QWidget* parent);

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
		QListView* m_listView{};
/*
		QSpinBox* m_masterPort{new QSpinBox(this)};
		QSpinBox* m_clientPort{new QSpinBox(this)};
		QLineEdit* m_clientName{new QLineEdit(this)};

		int m_previousMasterPort{};
		int m_previousClientPort{};
		QString m_previousClientName{};
*/};
