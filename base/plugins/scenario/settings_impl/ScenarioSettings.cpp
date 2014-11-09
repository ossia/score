#include "ScenarioSettings.hpp"
#include "ScenarioSettingsModel.hpp"
#include "ScenarioSettingsView.hpp"
#include "ScenarioSettingsPresenter.hpp"

using namespace iscore;

/////////////// HELLO WORLD SETTINGS CLASS ////////////////////
ScenarioSettings::ScenarioSettings()
{
}

SettingsDelegateViewInterface* ScenarioSettings::makeView()
{
	return new ScenarioSettingsView(nullptr);
}

SettingsDelegatePresenterInterface* ScenarioSettings::makePresenter(SettingsPresenter* p,
														  SettingsDelegateModelInterface* m,
														  SettingsDelegateViewInterface* v)
{
	auto pres = new ScenarioSettingsPresenter(p, m, v);
	m->setPresenter(pres);
	v->setPresenter(pres);

	return pres;
}

SettingsDelegateModelInterface* ScenarioSettings::makeModel()
{
	return new ScenarioSettingsModel();
}
