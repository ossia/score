#include <core/settings/Settings.hpp>
#include <core/settings/SettingsModel.hpp>
#include <core/settings/SettingsView.hpp>
#include <core/settings/SettingsPresenter.hpp>
#include <core/application/Application.hpp>
using namespace iscore;

Settings::Settings(QObject* parent):
	QObject{parent},
	m_settingsModel{new SettingsModel(this)},
	m_settingsView{new SettingsView(nullptr)},
	m_settingsPresenter{new SettingsPresenter(m_settingsModel,
											  m_settingsView,
											  this)}
{
}

void Settings::setupSettingsPlugin(SettingsDelegateFactoryInterface* plugin)
{
	auto model = plugin->makeModel();
	auto view = plugin->makeView();

	// Ownership transfer
	m_settingsPresenter->addSettingsPresenter(plugin->makePresenter(m_settingsPresenter,
																	model,
																	view));
	m_settingsModel->addSettingsModel(model);
	m_settingsView->addSettingsView(view);
}
