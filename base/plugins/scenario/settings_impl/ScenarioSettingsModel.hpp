#pragma once
#include <interface/settings/SettingsGroupModel.hpp>
#include <QString>
#include <QObject>

namespace iscore
{
	class SettingsGroupPresenter;
}
class ScenarioSettingsModel : public iscore::SettingsGroupModel
{
		Q_OBJECT
	public:
		ScenarioSettingsModel();

		void setText(QString txt);
		QString getText() const;

		virtual void setPresenter(iscore::SettingsGroupPresenter* presenter) override;

	signals:
		void textChanged();

	private:
		QString helloText;

		// SettingsGroupModel interface
	public:
		virtual void setFirstTimeSettings()
		{
		}
};
