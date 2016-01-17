#include "NetworkSettings.hpp"
#include "NetworkSettingsModel.hpp"
#include "NetworkSettingsPresenter.hpp"
#include "NetworkSettingsView.hpp"

namespace iscore {
class SettingsPresenter;
}  // namespace iscore

using namespace iscore;
namespace Network
{
NetworkSettings::NetworkSettings()
{
}

SettingsDelegateViewInterface* NetworkSettings::makeView()
{
    return new NetworkSettingsView(nullptr);
}

SettingsDelegatePresenterInterface* NetworkSettings::makePresenter(SettingsPresenter* p,
        SettingsDelegateModelInterface* m,
        SettingsDelegateViewInterface* v)
{
    auto pres = new NetworkSettingsPresenter(p, m, v);

    v->setPresenter(pres);

    pres->load();
    pres->view()->doConnections();

    return pres;
}

SettingsDelegateModelInterface* NetworkSettings::makeModel()
{
    return new NetworkSettingsModel();
}
}
