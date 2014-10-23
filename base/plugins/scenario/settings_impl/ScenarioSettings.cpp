#include "ScenarioSettings.hpp"
#include "ScenarioSettingsModel.hpp"
#include "ScenarioSettingsView.hpp"
#include "ScenarioSettingsPresenter.hpp"

using namespace iscore;

/////////////// HELLO WORLD SETTINGS CLASS ////////////////////
ScenarioSettings::ScenarioSettings()
{
}

SettingsGroupView* ScenarioSettings::makeView()
{
	return new ScenarioSettingsView(nullptr);
}

SettingsGroupPresenter* ScenarioSettings::makePresenter(SettingsPresenter* p,
														  SettingsGroupModel* m,
														  SettingsGroupView* v)
{
	auto pres = new ScenarioSettingsPresenter(p, m, v);
	m->setPresenter(pres);
	v->setPresenter(pres);

	return pres;
}

SettingsGroupModel* ScenarioSettings::makeModel()
{
	return new ScenarioSettingsModel();
}
