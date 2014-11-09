#pragma once
#include <interface/settingsdelegate/SettingsDelegateModelInterface.hpp>
#include <QString>
#include <QObject>

namespace iscore
{
	class SettingsDelegatePresenterInterface;
}
class ScenarioSettingsModel : public iscore::SettingsDelegateModelInterface
{
		Q_OBJECT
	public:
		ScenarioSettingsModel();

		void setText(QString txt);
		QString getText() const;

		virtual void setPresenter(iscore::SettingsDelegatePresenterInterface* presenter) override;

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
