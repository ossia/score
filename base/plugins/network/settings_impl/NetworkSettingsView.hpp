#pragma once
#include <QWidget>
#include <QLineEdit>
#include <interface/settings/SettingsGroup.hpp>
#include <core/presenter/command/Command.hpp>

class NetworkSettingsPresenter;
class NetworkSettingsView : public QWidget, public iscore::SettingsGroupView
{
		Q_OBJECT
	public:
		NetworkSettingsView(QWidget* parent);
		virtual void setPresenter(iscore::SettingsGroupPresenter* presenter) override;

		void setText(QString text);
		virtual QWidget* getWidget() override;

	signals:
		void submitCommand(iscore::Command* cmd);

	public slots:
		void on_textChanged();

	private:
		NetworkSettingsPresenter* m_presenter;
		QLineEdit* m_lineEdit; // Ownership goes to the widget parent class.
		QString m_previousText;

		// SettingsGroupView interface
	public:

};
