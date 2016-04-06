#include <core/settings/Settings.hpp>
#include <core/settings/SettingsModel.hpp>
#include <core/settings/SettingsPresenter.hpp>
#include <core/settings/SettingsView.hpp>

#include <iscore/plugins/settingsdelegate/SettingsDelegateFactoryInterface.hpp>

namespace iscore
{
Settings::Settings(QObject* parent) :
    QObject {parent},
    m_settingsModel {new SettingsModel(this) },
    m_settingsView {new SettingsView(nullptr) },
    m_settingsPresenter {new SettingsPresenter(m_settingsModel,
                                               m_settingsView,
                                               this)
}
{
}

Settings::~Settings()
{
    m_settingsView->deleteLater();
}

void Settings::setupSettingsPlugin(SettingsDelegateFactory& plugin)
{
    auto model = plugin.makeModel();
    if(model)
        m_settingsModel->addSettingsModel(model);

    auto view = plugin.makeView();
    if(!view)
        return;

    auto pres = plugin.makePresenter(
                    *model,
                    *view,
                    m_settingsPresenter);
    if(!pres)
        delete view;

    else
    {
        // Ownership transfer
        m_settingsPresenter->addSettingsPresenter(pres);
        m_settingsView->addSettingsView(view);
    }

}
}
