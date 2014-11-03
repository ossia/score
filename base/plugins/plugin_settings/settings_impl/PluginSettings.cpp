#include "PluginSettings.hpp"
#include "PluginSettingsModel.hpp"
#include "PluginSettingsView.hpp"
#include "PluginSettingsPresenter.hpp"

using namespace iscore;

PluginSettings::PluginSettings()
{
}

SettingsGroupView* PluginSettings::makeView()
{
	return new PluginSettingsView(nullptr);
}

SettingsGroupPresenter* PluginSettings::makePresenter(SettingsPresenter* p,
													   SettingsGroupModel* m,
													   SettingsGroupView* v)
{
	auto pres = new PluginSettingsPresenter(p, m, v);
	m->setPresenter(pres);
	v->setPresenter(pres);

	pres->load();
	pres->view()->doConnections();

	return pres;
}

SettingsGroupModel* PluginSettings::makeModel()
{
	return new PluginSettingsModel();
}
