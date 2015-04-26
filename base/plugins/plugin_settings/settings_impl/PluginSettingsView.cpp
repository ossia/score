#include "PluginSettingsView.hpp"
#include <QGridLayout>
#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsModel.hpp"
/*#include "commands/ClientPortChangedCommand.hpp"
#include "commands/MasterPortChangedCommand.hpp"
#include "commands/ClientNameChangedCommand.hpp"
*/
#include <QApplication>
using namespace iscore;

PluginSettingsView::PluginSettingsView(QObject* parent) :
    iscore::SettingsDelegateViewInterface {parent}
{
    auto layout = new QGridLayout(m_widget);
    m_widget->setLayout(layout);

    layout->addWidget(m_listView);
}

QWidget* PluginSettingsView::getWidget()
{
    return m_widget;
}

void PluginSettingsView::load()
{

}

void PluginSettingsView::doConnections()
{

}

PluginSettingsPresenter* PluginSettingsView::presenter()
{
    return static_cast<PluginSettingsPresenter*>(m_presenter);
}
