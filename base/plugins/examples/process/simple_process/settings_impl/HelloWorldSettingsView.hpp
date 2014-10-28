#pragma once
#include <QWidget>
#include <QLineEdit>
#include <interface/settings/SettingsGroup.hpp>
#include <core/presenter/command/Command.hpp>

class HelloWorldSettingsPresenter;
class HelloWorldSettingsView : public QWidget, public iscore::SettingsGroupView
{
		Q_OBJECT
	public:
		HelloWorldSettingsView(QWidget* parent);
		void setText(QString text);
		virtual QWidget* getWidget() override;

	signals:
		void submitCommand(iscore::Command* cmd);

	public slots:
		void on_textChanged();

	private:
		QLineEdit* m_lineEdit; // Ownership goes to the widget parent class.
		QString m_previousText;
};
