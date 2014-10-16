#pragma once
#include <interface/settings/SettingsGroup.hpp>
#include <core/presenter/Command.hpp>
#include <QObject>

class HelloWorldSettingsPresenter : public QObject, public iscore::SettingsGroupPresenter
{
		Q_OBJECT
	public:
		HelloWorldSettingsPresenter(iscore::SettingsPresenter* parent,
									iscore::SettingsGroupModel* model,
									iscore::SettingsGroupView* view);

		void setText(QString text);

		virtual void on_accept() override;
		virtual void on_reject() override;

	public slots:
		void updateViewText();
		void submitCommand(iscore::Command* cmd);

	private:
		// S'il y avait plusieurs contrôles chaque contrôle devrait avoir sa "commande".
		iscore::Command* m_currentCommand{nullptr};
};
