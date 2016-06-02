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

iscore::SettingsDelegateView* PluginSettingsFactory::makeView()
{
    return new PluginSettingsView(nullptr);
}

iscore::SettingsDelegatePresenter* PluginSettingsFactory::makePresenter_impl(
        iscore::SettingsDelegateModel& m,
        iscore::SettingsDelegateView& v,
        QObject* parent)
{
    return new PluginSettingsPresenter(m, v, parent);
}

iscore::SettingsDelegateModel* PluginSettingsFactory::makeModel()
{
    return new PluginSettingsModel();
}
}
