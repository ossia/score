#include "PluginSettings.hpp"
#include "PluginSettingsModel.hpp"
#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsView.hpp"

namespace iscore {
class SettingsPresenter;
}  // namespace iscore

using namespace iscore;

namespace PluginSettings
{
PluginSettingsFactory::PluginSettingsFactory()
{
}

SettingsDelegateViewInterface* PluginSettingsFactory::makeView()
{
    return new PluginSettingsView(nullptr);
}

SettingsDelegatePresenterInterface* PluginSettingsFactory::makePresenter(SettingsPresenter* p,
        SettingsDelegateModelInterface* m,
        SettingsDelegateViewInterface* v)
{
    auto pres = new PluginSettingsPresenter(p, m, v);

    v->setPresenter(pres);

    pres->load();
    pres->view()->doConnections();

    return pres;
}

SettingsDelegateModelInterface* PluginSettingsFactory::makeModel()
{
    return new PluginSettingsModel();
}
}
