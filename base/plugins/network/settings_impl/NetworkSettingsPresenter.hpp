#pragma once
#include <interface/settings/SettingsGroupPresenter.hpp>
#include <QObject>

class NetworkSettingsModel;
class NetworkSettingsView;
class MasterPortChangedCommand;
class ClientPortChangedCommand;
class ClientNameChangedCommand;
class NetworkSettingsPresenter : public iscore::SettingsGroupPresenter
{
		Q_OBJECT
	public:
		NetworkSettingsPresenter(iscore::SettingsPresenter* parent,
								 iscore::SettingsGroupModel* model,
								 iscore::SettingsGroupView* view);

		virtual void on_accept() override;
		virtual void on_reject() override;

		void load();

	public slots:
		void updateMasterPort();
		void updateClientPort();
		void updateClientName();

		void setMasterPortCommand(MasterPortChangedCommand* cmd);
		void setClientPortCommand(ClientPortChangedCommand* cmd);
		void setClientNameCommand(ClientNameChangedCommand* cmd);

	private:
		NetworkSettingsModel* model();
		NetworkSettingsView* view();
		// S'il y avait plusieurs contrôles chaque contrôle devrait avoir sa "commande".
		iscore::Command* m_masterportCommand{nullptr};
		iscore::Command* m_clientportCommand{nullptr};
		iscore::Command* m_clientnameCommand{nullptr};

		// SettingsGroupPresenter interface
	public:
		virtual QString settingsName() override
		{
			return tr("Network");
		}
		virtual QIcon settingsIcon() override;
};
