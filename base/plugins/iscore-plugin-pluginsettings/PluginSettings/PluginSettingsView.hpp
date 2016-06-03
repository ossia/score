#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <QListView>
#include <QWidget>

class QObject;

namespace PluginSettings
{
class PluginSettingsPresenter;
class PluginSettingsView : public iscore::SettingsDelegateView
{
        Q_OBJECT
    public:
        PluginSettingsView(QObject* parent);

        QListView* view()
        {
            return m_listView;
        }

        QWidget* getWidget() override;

    private:
        QWidget* m_widget {new QWidget};
        QListView* m_listView {new QListView{m_widget}};
};
}
