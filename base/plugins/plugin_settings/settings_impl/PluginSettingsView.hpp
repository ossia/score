#pragma once
#include <plugin_interface/settingsdelegate/SettingsDelegateViewInterface.hpp>
#include <public_interface/command/Command.hpp>
#include <QListView>

class PluginSettingsPresenter;
class PluginSettingsView : public iscore::SettingsDelegateViewInterface
{
        Q_OBJECT
    public:
        PluginSettingsView(QObject* parent);

        QListView* view()
        {
            return m_listView;
        }

        virtual QWidget* getWidget() override;
        void load();
        void doConnections();

    private:
        PluginSettingsPresenter* presenter();

        QWidget* m_widget {new QWidget};
        QListView* m_listView {new QListView{m_widget}};
};
