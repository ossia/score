#include "HelloWorldSettings.hpp"
#include "HelloWorldSettingsModel.hpp"
#include "HelloWorldSettingsView.hpp"
#include "HelloWorldSettingsPresenter.hpp"

using namespace iscore;

/////////////// HELLO WORLD SETTINGS CLASS ////////////////////
HelloWorldSettings::HelloWorldSettings()
{
}

std::unique_ptr<SettingsGroupView> HelloWorldSettings::makeView()
{
	return std::make_unique<HelloWorldSettingsView>(nullptr);
}

std::unique_ptr<SettingsGroupPresenter> HelloWorldSettings::makePresenter(SettingsGroupModel* m,
																		  SettingsGroupView* v)
{
	auto pres = std::make_unique<HelloWorldSettingsPresenter>(m, v);
	m->setPresenter(pres.get());
	v->setPresenter(pres.get());

	return std::move(pres);
}

std::unique_ptr<SettingsGroupModel> HelloWorldSettings::makeModel()
{
	return std::make_unique<HelloWorldSettingsModel>();
}
