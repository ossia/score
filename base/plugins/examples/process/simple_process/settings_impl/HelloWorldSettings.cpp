#include "HelloWorldSettings.hpp"
#include "HelloWorldSettingsModel.hpp"
#include "HelloWorldSettingsView.hpp"
#include "HelloWorldSettingsPresenter.hpp"

using namespace iscore;

/////////////// HELLO WORLD SETTINGS CLASS ////////////////////
HelloWorldSettings::HelloWorldSettings()
{
}

SettingsGroupView* HelloWorldSettings::makeView()
{
	return new HelloWorldSettingsView(nullptr);
}

SettingsGroupPresenter* HelloWorldSettings::makePresenter(SettingsPresenter* p,
														  SettingsGroupModel* m,
														  SettingsGroupView* v)
{
	auto pres = new HelloWorldSettingsPresenter(p, m, v);
	m->setPresenter(pres);
	v->setPresenter(pres);

	return pres;
}

SettingsGroupModel* HelloWorldSettings::makeModel()
{
	return new HelloWorldSettingsModel();
}
