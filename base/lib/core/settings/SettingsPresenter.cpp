#include <core/settings/SettingsPresenter.hpp>
#include <core/settings/SettingsView.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>


namespace iscore
{
SettingsPresenter::SettingsPresenter(
        SettingsView* view,
        QObject* parent) :
    QObject {parent},
        m_view {view}
{
    connect(m_view, &SettingsView::accepted,
    this,   &SettingsPresenter::on_accept);
    connect(m_view, &SettingsView::rejected,
    this,   &SettingsPresenter::on_reject);
}

void SettingsPresenter::addSettingsPresenter(SettingsDelegatePresenter* presenter)
{
    m_pluginPresenters.insert(presenter);
}

void SettingsPresenter::on_accept()
{
    for(auto& presenter : m_pluginPresenters)
    {
        presenter->on_accept();
    }
}

void SettingsPresenter::on_reject()
{
    for(auto& presenter : m_pluginPresenters)
    {
        presenter->on_reject();
    }
}
}
