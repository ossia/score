#include <QGridLayout>

#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsView.hpp"
#include <iscore/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <QHeaderView>

#include <QNetworkRequest>

#include <QJsonDocument>
#include <QNetworkReply>
class QObject;

namespace PluginSettings
{
PluginSettingsView::PluginSettingsView()
{
    {
        auto local_widget = new QWidget;
        auto local_layout = new QGridLayout{local_widget};
        local_widget->setLayout(local_layout);

        m_addonsOnSystem->horizontalHeader()->hide();
        m_addonsOnSystem->verticalHeader()->hide();
        m_addonsOnSystem->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_addonsOnSystem->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_addonsOnSystem->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_addonsOnSystem->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_addonsOnSystem->setSelectionMode(QAbstractItemView::SingleSelection);
        m_addonsOnSystem->setShowGrid(false);

        local_layout->addWidget(m_addonsOnSystem);

        m_widget->addTab(local_widget, tr("Local"));
    }

    {
        auto remote_widget = new QWidget;
        auto remote_layout = new QGridLayout{remote_widget};
        remote_layout->addWidget(m_remoteAddons, 0, 0, 2, 1);

        auto vlay = new QVBoxLayout;
        vlay->addWidget(m_refresh);
        vlay->addWidget(m_install);
        vlay->addStretch();
        remote_layout->addLayout(vlay, 0, 1, 1, 1);

        m_widget->addTab(remote_widget, tr("Browse"));
    }

    connect(&mgr, &QNetworkAccessManager::finished,
            this, [] (QNetworkReply* rep) {
       auto res = rep->readAll();
       auto json = QJsonDocument::fromJson(res);
       qDebug() << json.object();
    });
    connect(m_refresh, &QPushButton::pressed,
            this, [this] () {
       QNetworkRequest rqst{QUrl("https://raw.githubusercontent.com/OSSIA/iscore-addons/master/addons.json")};
       mgr.get(rqst);
    });
}

QWidget* PluginSettingsView::getWidget()
{
    return m_widget;
}
}
