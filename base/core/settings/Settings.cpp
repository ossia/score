#include <core/settings/Settings.hpp>
#include <core/settings/SettingsModel.hpp>
#include <core/settings/SettingsView.hpp>
#include <core/settings/SettingsPresenter.hpp>
using namespace iscore;

Settings::Settings():
	m_settingsModel{std::make_unique<SettingsModel>()},
	m_settingsView{std::make_unique<SettingsView>()},
	m_settingsPresenter{std::make_unique<SettingsPresenter>(m_settingsModel.get(), m_settingsView.get())}
{
}

void Settings::addSettingsPlugin(std::unique_ptr<iscore::SettingsGroup>&& plugin)
{
	auto model = plugin->makeModel();
	auto view = plugin->makeView();
	auto presenter = plugin->makePresenter(m_settingsPresenter.get(),
										   model.get(),
										   view.get());

	// Ownership transfer
	m_settingsModel->addSettingsModel(std::move(model));
	m_settingsView->addSettingsView(std::move(view));
	m_settingsPresenter->addSettingsPresenter(std::move(presenter));

	// Is this really necessary ?
	// m_plugins.insert(std::move(plugin));
}
