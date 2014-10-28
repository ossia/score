#pragma once
#include <interface/settings/SettingsGroupPresenter.hpp>
#include <core/presenter/command/Command.hpp>
#include <QObject>

class ScenarioSettingsPresenter : public iscore::SettingsGroupPresenter
{
		Q_OBJECT
	public:
		ScenarioSettingsPresenter(iscore::SettingsPresenter* parent,
									iscore::SettingsGroupModel* model,
									iscore::SettingsGroupView* view);

		void setText(QString text);

		virtual void on_accept() override;
		virtual void on_reject() override;

	public slots:
		void updateViewText();
		void on_submitCommand(iscore::Command* cmd);

	private:
		// S'il y avait plusieurs contrôles chaque contrôle devrait avoir sa "commande".
		iscore::Command* m_currentCommand{nullptr};

		// SettingsGroupPresenter interface
	public:
		virtual QString settingsName()
		{
			return "Scenario plugin";
		}
		virtual QIcon settingsIcon();
};
