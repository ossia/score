#pragma once
#include <interface/settingsdelegate/SettingsDelegateModelInterface.hpp>
#include <QStandardItemModel>
#include <QString>
#include <QObject>

namespace iscore
{
	class SettingsDelegatePresenterInterface;
}

class PluginSettingsPresenter;
class PluginSettingsModel : public iscore::SettingsDelegateModelInterface
{
		Q_OBJECT
	public:
		PluginSettingsModel();
		QStandardItemModel* model() { return m_plugins; }

		virtual void setPresenter(iscore::SettingsDelegatePresenterInterface* presenter) override;
		virtual void setFirstTimeSettings() override;
		PluginSettingsPresenter* presenter() { return m_presenter; }

	public slots:
		void on_itemChanged(QStandardItem*);

	private:
		QStandardItemModel* m_plugins{};
		PluginSettingsPresenter* m_presenter;
};
