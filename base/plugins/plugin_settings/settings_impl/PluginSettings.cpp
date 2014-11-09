#include "PluginSettings.hpp"
#include "PluginSettingsModel.hpp"
#include "PluginSettingsView.hpp"
#include "PluginSettingsPresenter.hpp"

using namespace iscore;

PluginSettings::PluginSettings()
{
}

SettingsDelegateViewInterface* PluginSettings::makeView()
{
	return new PluginSettingsView(nullptr);
}

SettingsDelegatePresenterInterface* PluginSettings::makePresenter(SettingsPresenter* p,
													   SettingsDelegateModelInterface* m,
													   SettingsDelegateViewInterface* v)
{
	auto pres = new PluginSettingsPresenter(p, m, v);
	m->setPresenter(pres);
	v->setPresenter(pres);

	pres->load();
	pres->view()->doConnections();

	return pres;
}

SettingsDelegateModelInterface* PluginSettings::makeModel()
{
	return new PluginSettingsModel();
}
