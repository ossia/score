#pragma once
#include <QWidget>
#include <QLineEdit>
#include <interface/settings/SettingsGroup.hpp>
#include <core/presenter/command/Command.hpp>
#include <QSpinBox>

class NetworkSettingsPresenter;
class NetworkSettingsView : public QWidget, public iscore::SettingsGroupView
{
		Q_OBJECT
	public:
		NetworkSettingsView(QWidget* parent);

		void setMasterPort(int val);
		void setClientPort(int val);
		void setClientName(QString text);

		virtual QWidget* getWidget() override;

	signals:
		void submitCommand(iscore::Command* cmd);

	public slots:
		void on_masterPortChanged(int);
		void on_clientPortChanged(int);
		void on_clientNameChanged();

	private:
		NetworkSettingsPresenter* presenter();

		QSpinBox* m_masterPort{new QSpinBox(this)};
		QSpinBox* m_clientPort{new QSpinBox(this)};
		QLineEdit* m_clientName{new QLineEdit(this)};

		int m_previousMasterPort{};
		int m_previousClientPort{};
		QString m_previousClientName{};
};
