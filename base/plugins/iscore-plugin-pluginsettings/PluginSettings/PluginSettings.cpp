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

iscore::SettingsDelegatePresenterInterface* PluginSettingsFactory::makePresenter_impl(
        iscore::SettingsDelegateModelInterface& m,
        iscore::SettingsDelegateViewInterface& v,
        QObject* parent)
{
    return new PluginSettingsPresenter(m, v, parent);
}

iscore::SettingsDelegateModelInterface* PluginSettingsFactory::makeModel()
{
    return new PluginSettingsModel();
}
}
