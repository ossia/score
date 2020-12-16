// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "View.hpp"

#include "Presenter.hpp"

#include <Library/LibrarySettings.hpp>
#include <PackageManager/FileDownloader.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <zipdownloader.hpp>

#include <QJsonDocument>
#include <wobjectimpl.h>
#include <score_git_info.hpp>

W_OBJECT_IMPL(PM::PluginSettingsView)
namespace PM
{

namespace zip_helper
{

QString get_path(const QString& str)
{
  auto idx = str.lastIndexOf('/');
  if (idx != -1)
  {
    return str.mid(0, idx);
  }
  return "";
}

QString slash_path(const QString& str)
{
  return {};
}

QString relative_path(const QString& base, const QString& filename)
{
  return filename;
}

QString combine_path(const QString& path, const QString& filename)
{
  return path + "/" + filename;
}

bool make_folder(const QString& str)
{
  QDir d;
  return d.mkpath(str);
}

}

PluginSettingsView::PluginSettingsView()
{
  m_progress->setMinimum(0);
  m_progress->setMaximum(0);
  m_progress->setHidden(true);
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
    vlay->addWidget(m_progress);
    vlay->addStretch();
    remote_layout->addLayout(vlay, 0, 1, 1, 1);

    m_widget->addTab(remote_widget, tr("Browse"));
  }

  for (QTableView* v : {m_addonsOnSystem, m_remoteAddons})
  {
    v->horizontalHeader()->hide();
    v->verticalHeader()->hide();
    v->verticalHeader()->sectionResizeMode(QHeaderView::Fixed);
    v->verticalHeader()->setDefaultSectionSize(40);
    v->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    v->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->setSelectionMode(QAbstractItemView::SingleSelection);
    v->setShowGrid(false);
  }

  connect(
      &mgr,
      &QNetworkAccessManager::finished,
      this,
      &PluginSettingsView::on_message);

  connect(m_refresh, &QPushButton::pressed, this, [this]() {
    RemotePackagesModel* model
        = static_cast<RemotePackagesModel*>(m_remoteAddons->model());
    model->clear();

    m_progress->setVisible(true);

    QNetworkRequest rqst{
        QUrl("https://raw.githubusercontent.com/OSSIA/score-addons/master/"
             "addons.json")};
    mgr.get(rqst);
  });

  connect(
      m_install, &QPushButton::pressed, this, &PluginSettingsView::install);
}

QWidget* PluginSettingsView::getWidget()
{
  return m_widget;
}

void PluginSettingsView::handleAddonList(const QJsonObject& obj)
{
  m_progress->setVisible(true);
  auto arr = obj["addons"].toArray();
  m_addonsToRetrieve = arr.size();
  for (QJsonValue elt : arr)
  {
    QNetworkRequest rqst{QUrl(elt.toString())};
    mgr.get(rqst);
  }
}

void PluginSettingsView::handleAddon(const QJsonObject& obj)
{
  m_addonsToRetrieve--;
  if (m_addonsToRetrieve == 0)
  {
    m_progress->setHidden(true);
  }

  auto addon = RemotePackage::fromJson(obj);
  if (!addon)
    return;

  auto& add = *addon;

  // Load images
  RemotePackagesModel* model
      = static_cast<RemotePackagesModel*>(m_remoteAddons->model());
  if (!add.smallImagePath.isEmpty())
  {
    // c.f. https://wiki.qt.io/Download_Data_from_URL
    auto dl = new score::FileDownloader{QUrl{add.smallImagePath}};
    connect(dl, &score::FileDownloader::downloaded, this, [=](QByteArray arr) {
      model->updateAddon(add.key, [=](RemotePackage& add) {
        add.smallImage.loadFromData(arr);
      });

      dl->deleteLater();
    });
  }

  if (!add.largeImagePath.isEmpty())
  {
    // c.f. https://wiki.qt.io/Download_Data_from_URL
    auto dl = new score::FileDownloader{QUrl{add.largeImagePath}};
    connect(dl, &score::FileDownloader::downloaded, this, [=](QByteArray arr) {
      model->updateAddon(add.key, [=](RemotePackage& add) {
        add.largeImage.loadFromData(arr);
      });

      dl->deleteLater();
    });
  }

  model->addAddon(std::move(add));
}

void PluginSettingsView::on_message(QNetworkReply* rep)
{
  auto res = rep->readAll();
  auto json = QJsonDocument::fromJson(res).object();

  if (json.contains("addons"))
  {
    handleAddonList(json);
  }
  else if (json.contains("name"))
  {
    handleAddon(json);
  }
  else
  {
    qDebug() << res;
    m_progress->setHidden(true);
  }
  rep->deleteLater();

}

void PluginSettingsView::install()
{
  auto& remotePlugins
      = *static_cast<RemotePackagesModel*>(m_remoteAddons->model());

  auto rows = m_remoteAddons->selectionModel()->selectedRows(0);
  if (rows.empty())
    return;

  auto num = rows.first().row();
  SCORE_ASSERT(int(remotePlugins.addons().size()) > num);
  auto& addon = remotePlugins.addons().at(num);

  m_progress->setVisible(true);

  if(addon.kind == "addon")
    installAddon(addon);
  else if(addon.kind == "sdk")
    installSDK(addon);
  else if(addon.kind == "media")
    installLibrary(addon);
}

void PluginSettingsView::installAddon(const RemotePackage& addon)
{
  if (addon.file == QUrl{})
  {
    m_progress->setHidden(true);
    return;
  }

  const QString addons_path{score::AppContext()
        .settings<Library::Settings::Model>()
        .getPath()
        + "/Addons"};
  zdl::download_and_extract(
        addon.file,
        QDir{addons_path}.absolutePath(),
        [=] (const std::vector<QString>& res) {
    m_progress->setHidden(true);
    if(res.empty())
      return;
    // We want the extracted folder to have the name of the addon
    {
      QDir addons_dir{addons_path};
      QFileInfo a_file(res[0]);
      auto d = a_file.dir();
      auto old_d = d;
      while (d.cdUp() && !d.isRoot())
      {
        if (d == addons_dir)
        {
          addons_dir.rename(old_d.dirName(), addon.raw_name);
          break;
        }
        old_d = d;
      }
    }

    QMessageBox::information(
          m_widget,
          tr("Addon downloaded"),
          tr("The addon %1 has been succesfully installed in :\n"
             "%2\n\n"
             "It will be built and enabled shortly.\nCheck the message "
             "console for errors if nothing happens.")
          .arg(addon.name)
          .arg(QFileInfo(addons_path).absoluteFilePath()));
  },
  [=] {
    m_progress->setHidden(true);
    QMessageBox::warning(
          m_widget,
          tr("Download failed"),
          tr("The package %1 could not be downloaded.").arg(addon.name));
  });
}

void PluginSettingsView::installSDK(const RemotePackage& addon)
{
  QString version = SCORE_TAG_NO_V;
  constexpr const char* platform =
    #if defined(_WIN32)
      "windows"
    #elif defined(__APPLE__)
      "mac"
    #else
      "linux"
    #endif
  ;

  const QString lib_path{score::AppContext()
        .settings<Library::Settings::Model>()
        .getPath()};
  const QString sdk_path{lib_path + "/SDK/" + version};
  QDir{lib_path}.mkpath(sdk_path);

  const QUrl sdk_url =
      QString("https://github.com/ossia/score/releases/download/%1/%2-sdk.zip")
      .arg(SCORE_TAG)
      .arg(platform);

  zdl::download_and_extract(
        sdk_url,
        QFileInfo{sdk_path}.absoluteFilePath(),
        [=] (const std::vector<QString>& res) {
    m_progress->setHidden(true);
    if(res.empty())
      return;

    QMessageBox::information(
          m_widget,
          tr("SDK downloaded"),
          tr("The SDK has been succesfully installed in the user library."));
  },
  [this] {
    m_progress->setHidden(true);
    QMessageBox::warning(
          m_widget,
          tr("Download failed"),
          tr("The SDK could not be downloaded."));
  });
}

void PluginSettingsView::installLibrary(const RemotePackage& addon)
{
  const QString destination{score::AppContext()
        .settings<Library::Settings::Model>()
        .getPath() + "/Media/" + addon.raw_name};

  QDir{}.mkpath(destination);

  zdl::download_and_extract(
        addon.file,
        QFileInfo{destination}.absoluteFilePath(),
        [=] (const std::vector<QString>& res) {
    m_progress->setHidden(true);
    if(res.empty())
      return;

    QMessageBox::information(
          m_widget,
          tr("Package downloaded"),
          tr("The package %1 has been succesfully installed in the user library.").arg(addon.name));
  },
  [=] {
    m_progress->setHidden(true);
    QMessageBox::warning(
          m_widget,
          tr("Download failed"),
          tr("The package %1 could not be downloaded.").arg(addon.name));
  });
}
}
