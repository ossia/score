#include "NetworkSettings.hpp"
#include "NetworkSettingsModel.hpp"
#include "NetworkSettingsView.hpp"
#include "NetworkSettingsPresenter.hpp"

using namespace iscore;

NetworkSettings::NetworkSettings()
{
}

SettingsGroupView* NetworkSettings::makeView()
{
	return new NetworkSettingsView(nullptr);
}

SettingsGroupPresenter* NetworkSettings::makePresenter(SettingsPresenter* p,
													   SettingsGroupModel* m,
													   SettingsGroupView* v)
{
	auto pres = new NetworkSettingsPresenter(p, m, v);
	m->setPresenter(pres);
	v->setPresenter(pres);

	pres->load();

	return pres;
}

SettingsGroupModel* NetworkSettings::makeModel()
{
	return new NetworkSettingsModel();
}
