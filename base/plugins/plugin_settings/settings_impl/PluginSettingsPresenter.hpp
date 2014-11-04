#pragma once
#include <interface/settings/SettingsGroupPresenter.hpp>
#include <QObject>

class PluginSettingsModel;
class PluginSettingsView;
class BlacklistCommand;
class PluginSettingsPresenter : public iscore::SettingsGroupPresenter
{
		Q_OBJECT
	public:
		PluginSettingsPresenter(iscore::SettingsPresenter* parent,
								iscore::SettingsGroupModel* model,
								iscore::SettingsGroupView* view);

		virtual void on_accept() override;
		virtual void on_reject() override;

		virtual QString settingsName() override
		{
			return tr("Plugin");
		}

		virtual QIcon settingsIcon() override;

		void load();
		PluginSettingsModel* model();
		PluginSettingsView* view();

	public slots:
		/*
		void updateMasterPort();
		void updateClientPort();
		void updateClientName();

		void setMasterPortCommand(MasterPortChangedCommand* cmd);
		void setClientPortCommand(ClientPortChangedCommand* cmd);
		void setClientNameCommand(ClientNameChangedCommand* cmd);
		*/

		void setBlacklistCommand(BlacklistCommand* cmd);
	private:
		// S'il y avait plusieurs contrôles chaque contrôle devrait avoir sa "commande".
		iscore::Command* m_blacklistCommand{nullptr};
};
