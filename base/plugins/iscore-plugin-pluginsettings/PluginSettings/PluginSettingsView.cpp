#include <QGridLayout>

#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsView.hpp"
#include <iscore/plugins/settingsdelegate/SettingsDelegateView.hpp>

class QObject;

namespace PluginSettings
{
PluginSettingsView::PluginSettingsView(QObject* parent) :
    iscore::SettingsDelegateView {parent}
{
    auto layout = new QGridLayout(m_widget);
    m_widget->setLayout(layout);

    layout->addWidget(m_listView);
}

QWidget* PluginSettingsView::getWidget()
{
    return m_widget;
}
}
