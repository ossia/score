#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <QTableView>
#include <QTabWidget>
#include <QGridLayout>
#include <QPushButton>
#include <QWidget>

#include <QNetworkAccessManager>
class QObject;

namespace PluginSettings
{
class PluginSettingsPresenter;
class PluginSettingsView : public iscore::SettingsDelegateView
{
        Q_OBJECT
    public:
        PluginSettingsView();

        QTableView* view()
        {
            return m_addonsOnSystem;
        }

        QWidget* getWidget() override;


    private:
        QTabWidget* m_widget = new QTabWidget;

        QTableView* m_addonsOnSystem{new QTableView};

        QTableView* m_remoteAddons{new QTableView};
        QPushButton* m_refresh{new QPushButton{tr("Refresh")}};
        QPushButton* m_install{new QPushButton{tr("Install")}};

        QNetworkAccessManager mgr;
};
}
