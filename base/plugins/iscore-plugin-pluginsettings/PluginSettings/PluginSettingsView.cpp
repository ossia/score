#include <QGridLayout>

#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsView.hpp"
#include <PluginSettings/FileDownloader.hpp>
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

    for(QTableView* v : {m_addonsOnSystem, m_remoteAddons})
    {
        v->horizontalHeader()->hide();
        v->verticalHeader()->hide();
        v->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        v->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        v->setSelectionBehavior(QAbstractItemView::SelectRows);
        v->setEditTriggers(QAbstractItemView::NoEditTriggers);
        v->setSelectionMode(QAbstractItemView::SingleSelection);
        v->setShowGrid(false);
    }

    connect(&mgr, &QNetworkAccessManager::finished,
            this, [this] (QNetworkReply* rep) {
       auto res = rep->readAll();
       auto json = QJsonDocument::fromJson(res).object();

       if(json.contains("addons"))
       {
           handleAddonList(json);
       }
       else if(json.contains("name"))
       {
            handleAddon(json);
       }

       rep->deleteLater();
    });

    connect(m_refresh, &QPushButton::pressed,
            this, [this] () {
       QNetworkRequest rqst{QUrl("https://raw.githubusercontent.com/OSSIA/iscore-addons/master/addons.json")};
       mgr.get(rqst);
    });

    connect(m_install, &QPushButton::pressed,
            this, [this] () {

    });

}

QWidget* PluginSettingsView::getWidget()
{
    return m_widget;
}

void PluginSettingsView::handleAddonList(const QJsonObject& obj)
{
    auto arr = obj["addons"].toArray();
    for(QJsonValue elt : arr)
    {
        QNetworkRequest rqst{QUrl(elt.toString())};
        mgr.get(rqst);
    }
}

void PluginSettingsView::handleAddon(const QJsonObject& obj)
{
    using Funmap = std::map<QString, std::function<void(QJsonValue)>>;

    RemoteAddon add;
    QString small, large;
    const Funmap funmap
    {
        { "name",    [&] (QJsonValue v) { add.name = v.toString(); } },
        { "version", [&] (QJsonValue v) { add.version = v.toString(); } },
        { "url",     [&] (QJsonValue v) { add.latestVersionAddress = v.toString(); } },
        { "short",   [&] (QJsonValue v) { add.shortDescription = v.toString(); } },
        { "long",    [&] (QJsonValue v) { add.longDescription = v.toString(); } },
        { "small",   [&] (QJsonValue v) { small = v.toString(); } },
        { "large",   [&] (QJsonValue v) { large = v.toString(); } },
        { "key",     [&] (QJsonValue v) { add.key = UuidKey<iscore::Addon>(v.toString().toLatin1().constData()); } }
    };

    for(const auto& k : obj.keys())
    {
        auto it = funmap.find(k);
        if(it != funmap.end())
        {
            it->second(obj[k]);
        }
    }

    if(add.key.impl().is_nil() || add.name.isEmpty())
    {
        return;
    }

    RemotePluginItemModel* model = static_cast<RemotePluginItemModel*>(m_remoteAddons->model());

    if(!small.isEmpty())
    {
        // c.f. https://wiki.qt.io/Download_Data_from_URL
        auto dl = new iscore::FileDownloader{QUrl{small}};
        connect(dl, &iscore::FileDownloader::downloaded,
                this, [=] (QByteArray arr) {
            model->updateAddon(add.key, [=] (RemoteAddon& add) {
                add.smallImage.loadFromData(arr);
            });

            dl->deleteLater();
        });
    }

    if(!large.isEmpty())
    {
        // c.f. https://wiki.qt.io/Download_Data_from_URL
        auto dl = new iscore::FileDownloader{QUrl{large}};
        connect(dl, &iscore::FileDownloader::downloaded,
                this, [=] (QByteArray arr) {
            model->updateAddon(add.key, [=] (RemoteAddon& add) {
                add.largeImage.loadFromData(arr);
            });

            dl->deleteLater();
        });
    }

    model->addAddon(std::move(add));
}
}
