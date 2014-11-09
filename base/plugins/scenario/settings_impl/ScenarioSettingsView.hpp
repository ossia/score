#pragma once
#include <QWidget>
#include <QLineEdit>
#include <interface/settingsdelegate/SettingsDelegateViewInterface.hpp>
#include <core/presenter/command/Command.hpp>

class ScenarioSettingsPresenter;
class ScenarioSettingsView : public iscore::SettingsDelegateViewInterface
{
		Q_OBJECT
	public:
		ScenarioSettingsView(QObject* parent);

		void setText(QString text);
		virtual QWidget* getWidget() override;

	signals:
		void submitCommand(iscore::Command* cmd);

	public slots:
		void on_textChanged();

	private:
		QWidget* m_widget{new QWidget};
		QLineEdit* m_lineEdit{new QLineEdit{m_widget}};
		QString m_previousText;

};
