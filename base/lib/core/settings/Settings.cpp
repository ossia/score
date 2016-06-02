#include <core/settings/Settings.hpp>
#include <core/settings/SettingsModel.hpp>
#include <core/settings/SettingsPresenter.hpp>
#include <core/settings/SettingsView.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateViewInterface.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactoryInterface.hpp>

namespace iscore
{
Settings::Settings() :
    m_settingsView {new SettingsView(nullptr) },
    m_settingsPresenter {new SettingsPresenter(m_settingsView,
                                               nullptr)}
{
}

Settings::~Settings()
{
    m_settingsView->deleteLater();
}

void Settings::setupSettingsPlugin(
        const iscore::ApplicationContext& ctx,
        SettingsDelegateFactory& plugin)
{
    auto model = plugin.makeModel(ctx);
    if(!model)
        return;

    auto& model_ref = *model;
    m_settings.push_back(std::move(model));

    auto view = plugin.makeView();
    if(!view)
        return;

    auto pres = plugin.makePresenter(
                model_ref,
                *view,
                m_settingsPresenter);
    if(pres)
    {
        // Ownership transfer
        m_settingsPresenter->addSettingsPresenter(pres);
        m_settingsView->addSettingsView(view);
    }
    else
    {
        delete view;
    }

}
}
