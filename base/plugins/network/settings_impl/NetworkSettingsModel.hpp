#pragma once
#include <interface/settings/SettingsGroupModel.hpp>
#include <QString>
#include <QObject>

namespace iscore
{
	class SettingsGroupPresenter;
}
class NetworkSettingsModel : public iscore::SettingsGroupModel
{
		Q_OBJECT
	public:
		NetworkSettingsModel();

		void setClientName(QString txt);
		QString getClientName() const;
		void setClientPort(int val);
		int getClientPort() const;
		void setMasterPort(int val);
		int getMasterPort() const;

		virtual void setPresenter(iscore::SettingsGroupPresenter* presenter) override;
		virtual void setFirstTimeSettings() override;

	signals:
		void clientNameChanged();
		void clientPortChanged();
		void masterPortChanged();

	private:
		int masterPort;
		int clientPort;
		QString clientName;
};
