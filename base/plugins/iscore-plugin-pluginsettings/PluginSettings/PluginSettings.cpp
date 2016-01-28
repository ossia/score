#include "PluginSettings.hpp"
#include "PluginSettingsModel.hpp"
#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsView.hpp"

namespace iscore {
class SettingsPresenter;
}  // namespace iscore

namespace PluginSettings
{
PluginSettingsFactory::PluginSettingsFactory()
{
}

iscore::SettingsDelegateViewInterface* PluginSettingsFactory::makeView()
{
    return new PluginSettingsView(nullptr);
}

iscore::SettingsDelegatePresenterInterface* PluginSettingsFactory::makePresenter(
        iscore::SettingsPresenter* p,
        iscore::SettingsDelegateModelInterface* m,
        iscore::SettingsDelegateViewInterface* v)
{
    auto pres = new PluginSettingsPresenter(p, m, v);

    v->setPresenter(pres);

    pres->load();
    pres->view()->doConnections();

    return pres;
}

iscore::SettingsDelegateModelInterface* PluginSettingsFactory::makeModel()
{
    return new PluginSettingsModel();
}
}
