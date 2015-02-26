#include "NetworkSettings.hpp"
#include "NetworkSettingsModel.hpp"
#include "NetworkSettingsView.hpp"
#include "NetworkSettingsPresenter.hpp"

using namespace iscore;

NetworkSettings::NetworkSettings()
{
}

SettingsDelegateViewInterface* NetworkSettings::makeView()
{
    return new NetworkSettingsView (nullptr);
}

SettingsDelegatePresenterInterface* NetworkSettings::makePresenter (SettingsPresenter* p,
        SettingsDelegateModelInterface* m,
        SettingsDelegateViewInterface* v)
{
    auto pres = new NetworkSettingsPresenter (p, m, v);

    v->setPresenter (pres);

    pres->load();
    pres->view()->doConnections();

    return pres;
}

SettingsDelegateModelInterface* NetworkSettings::makeModel()
{
    return new NetworkSettingsModel();
}
