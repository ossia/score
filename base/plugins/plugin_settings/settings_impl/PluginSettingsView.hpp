#pragma once
#include <interface/settingsdelegate/SettingsDelegateViewInterface.hpp>
#include <core/presenter/command/Command.hpp>
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

    signals:
        void submitCommand(iscore::Command* cmd);

    public slots:

    private:
        PluginSettingsPresenter* presenter();

        QWidget* m_widget {new QWidget};
        QListView* m_listView {new QListView{m_widget}};
};
