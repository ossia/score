// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "View.hpp"

#include "Presenter.hpp"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QDesktopServices>

#include <PackageManager/FileDownloader.hpp>
#include <score_git_info.hpp>
#include <wobjectimpl.h>
#include <zipdownloader.hpp>

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
  storage = QStorageInfo(score::AppContext().settings<Library::Settings::Model>().getPackagesPath());

  m_progress->setMinimum(0);
  m_progress->setMaximum(100);
  m_progress->setHidden(true);

  auto grid = new QGridLayout{m_widget};
  grid->setContentsMargins(0, 0, 0, 0);
  m_widget->setLayout(grid);

  auto tab_widget = new QTabWidget;
  grid->addWidget(tab_widget, 0, 0);
  grid->addWidget(m_progress, 1, 0);

  {
    auto local_widget = new QWidget;
    auto local_layout = new QVBoxLayout{local_widget};
    local_widget->setLayout(local_layout);
    local_layout->addWidget(m_addonsOnSystem);

    tab_widget->addTab(local_widget, tr("Local"));
  }

  {
    auto remote_widget = new QWidget;
    auto remote_layout = new QVBoxLayout{remote_widget};
    remote_widget->setLayout(remote_layout);
    remote_layout->addWidget(m_remoteAddons);

    tab_widget->addTab(remote_widget, tr("Browse"));
  }

  auto side_widget = new QWidget;
  auto vlay = new QVBoxLayout{side_widget};
  grid->addWidget(side_widget, 0, 1, 2, 1);

  vlay->addWidget(m_link);
  vlay->addWidget(m_uninstall);
  m_install->setVisible(false);
  vlay->addWidget(m_install);
  vlay->addStretch();

  set_info();
  vlay->addWidget(m_storage);

  for (QTableView* v : {m_addonsOnSystem, m_remoteAddons})
  {
    v->verticalHeader()->hide();
    v->verticalHeader()->sectionResizeMode(QHeaderView::Fixed);
    v->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    v->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    v->setSelectionBehavior(QAbstractItemView::SelectRows);
    v->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->setSelectionMode(QAbstractItemView::SingleSelection);
    v->setShowGrid(false);
  }

  connect(tab_widget, &QTabWidget::tabBarClicked, this, [this] (int i) {
    if (i)
    {
      m_uninstall->setVisible(false);
      m_install->setVisible(true);

      RemotePackagesModel* model
          = static_cast<RemotePackagesModel*>(m_remoteAddons->model());
      model->clear();

      m_progress->setVisible(true);
      m_progress->setValue(0);

      QNetworkRequest rqst{
        QUrl("https://raw.githubusercontent.com/ossia/score-addons/master/"
             "addons.json")};
      mgr.get(rqst);
    }
    else
    {
      m_uninstall->setVisible(true);
      m_install->setVisible(false);
    }
  });

  connect(m_link, &QPushButton::pressed,
          this, &PluginSettingsView::openLink);

  connect(m_uninstall, &QPushButton::pressed,
          this, &PluginSettingsView::uninstall);

  connect(m_install, &QPushButton::pressed,
          this, &PluginSettingsView::install);

  connect(&mgr, &QNetworkAccessManager::finished,
          this, &PluginSettingsView::on_message);
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
  RemotePackagesModel* model
      = static_cast<RemotePackagesModel*>(m_remoteAddons->model());

  if (m_addonsToRetrieve == std::ssize(model->m_vec))
  {
    reset_progress();
  }
  else
    m_progress->setValue(m_progress->value() + (100 / m_addonsToRetrieve));


  auto addon = Package::fromJson(obj);
  if (!addon)
    return;

  auto& add = *addon;

  // Load images
  if (!add.smallImagePath.isEmpty())
  {
    // c.f. https://wiki.qt.io/Download_Data_from_URL
    auto dl = new score::FileDownloader{QUrl{add.smallImagePath}};
    connect(dl, &score::FileDownloader::downloaded, this, [=](QByteArray arr) {
      model->updateAddon(add.key, [=](Package& add) {
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
      model->updateAddon(add.key, [=](Package& add) {
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
    qDebug() << rep->request().url().toString() << ' ' << res;
    reset_progress();
  }
  rep->deleteLater();
}

// the install button set to visible means we are browsing
PackagesModel* PluginSettingsView::getCurrentModel()
{
  if (m_install->isVisible())
    return static_cast<PackagesModel*>(m_remoteAddons->model());
  else
    return static_cast<PackagesModel*>(m_addonsOnSystem->model());
}

int PluginSettingsView::getCurrentRow(const QTableView* t = nullptr)
{
  QModelIndexList rows{};

  if (t)
    rows = t->selectionModel()->selectedRows(0);
  else
  {
    if (m_install->isVisible())
      rows = m_remoteAddons->selectionModel()->selectedRows(0);
    else
      rows = m_addonsOnSystem->selectionModel()->selectedRows(0);
  }

  if (rows.isEmpty())
    return -1;

  return rows.first().row();
}

Package PluginSettingsView::selectedPackage(const PackagesModel* model, int row)
{
  if (row == -1)
    return {};

  SCORE_ASSERT(int(model->addons().size()) > row);

  return model->addons().at(row);
}

void PluginSettingsView::openLink()
{
  const auto& addon = selectedPackage(getCurrentModel(), getCurrentRow());

  QDesktopServices::openUrl(addon.url);
}

void PluginSettingsView::install()
{
  const auto& addon =
      selectedPackage(static_cast<PackagesModel*>(m_remoteAddons->model()), getCurrentRow(m_remoteAddons));

  m_progress->setVisible(true);

  if (addon.kind == "addon" || addon.kind == "nodes")
    installAddon(addon);
  else if (addon.kind == "sdk")
    installSDK(addon);
  else if (addon.kind == "media")
    installLibrary(addon);
}

void PluginSettingsView::uninstall()
{
  const auto& addon =
      selectedPackage(static_cast<PackagesModel*>(m_addonsOnSystem->model()), getCurrentRow(m_addonsOnSystem));

  bool success{false};

  const auto& library{
    score::AppContext().settings<Library::Settings::Model>()};

  if (addon.kind == "addon" || addon.kind == "nodes" || addon.kind == "media")
  {
    success = QDir{library.getPackagesPath() + '/' + addon.raw_name}.removeRecursively();
  }
  else if (addon.kind == "sdk")
  {
    success = QDir{library.getSDKPath()}.removeRecursively();
  }

  if (success)
  {
    const auto& localPlugins
        = static_cast<LocalPackagesModel*>(m_addonsOnSystem->model());

    localPlugins->removeAddon(addon);
    set_info();
  }
}

void PluginSettingsView::installAddon(const Package& addon)
{
  if (addon.file == QUrl{})
  {
    reset_progress();
    return;
  }

  const QString& installPath = score::AppContext().settings<Library::Settings::Model>().getPackagesPath();
  zdl::download_and_extract(
        addon.file,
        QDir{installPath}.absolutePath(),
        [=](const std::vector<QString>& res) {
    reset_progress();
    if (res.empty())
      return;
    // We want the extracted folder to have the name of the addon
    {
      QDir addons_dir{installPath};
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
          tr("The addon %1 has been successfully installed in :\n"
             "%2\n\n"
             "It will be built and enabled shortly.\nCheck the message "
             "console for errors if nothing happens.")
          .arg(addon.name)
          .arg(QFileInfo(installPath).absoluteFilePath()));
  },
  [=](qint64 recieved, qint64 total) { progress_from_bytes(recieved, total); },
  [=] {
    reset_progress();
    QMessageBox::warning(
          m_widget,
          tr("Download failed"),
          tr("The package %1 could not be downloaded.").arg(addon.name));
  });
}

void PluginSettingsView::installSDK(const Package& addon)
{
  constexpr const char* platform =
    #if defined(_WIN32)
      "windows"
    #elif defined(__APPLE__)
      "mac"
    #else
      "linux"
    #endif
      ;

  const QString sdk_path{
    score::AppContext().settings<Library::Settings::Model>().getSDKPath() + '/' + SCORE_TAG_NO_V};
  QDir{}.mkpath(sdk_path);

  const QUrl sdk_url
      = QString(
        "https://github.com/ossia/score/releases/download/%1/%2-sdk.zip")
      .arg(SCORE_TAG)
      .arg(platform);

  zdl::download_and_extract(
        sdk_url,
        QFileInfo{sdk_path}.absoluteFilePath(),
        [=](const std::vector<QString>& res) {
    reset_progress();
    if (res.empty())
      return;

    QMessageBox::information(
          m_widget,
          tr("SDK downloaded"),
          tr("The SDK has been successfully installed in the user library."));

    set_info();
  },
  [=](qint64 recieved, qint64 total) { progress_from_bytes(recieved, total); },
  [this] {
    reset_progress();
    QMessageBox::warning(
          m_widget,
          tr("Download failed"),
          tr("The SDK could not be downloaded."));
  });
}

void PluginSettingsView::installLibrary(const Package& addon)
{
  const QString destination{
    score::AppContext().settings<Library::Settings::Model>().getPackagesPath()
        + "/" + addon.raw_name};

  QDir{}.mkpath(destination);

  zdl::download_and_extract(
        addon.file,
        QFileInfo{destination}.absoluteFilePath(),
        [=] (const std::vector<QString>& res) { on_packageInstallSuccess(addon, destination, res); },
  [=](qint64 recieved, qint64 total) { progress_from_bytes(recieved, total); },
  [=] { on_packageInstallFailure(addon); });
}

void PluginSettingsView::on_packageInstallSuccess(
      const Package& addon,
      const QDir& destination,
      const std::vector<QString>& res)
{
  reset_progress();
  if (res.empty())
    return;

  // Often zip files contain a single, empty directory.
  // In that case, we move everything up a level to make the library cleaner.
  QDir dir{destination};
  auto files = dir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
  if(files.size() == 1)
  {
    auto child = files[0];
    QFileInfo info{dir.absoluteFilePath(child)};
    if(info.isDir()) {
      dir.rename(child, "___score_tmp___");
      QDir subdir{dir.absoluteFilePath("___score_tmp___")};

      for(auto& entry : subdir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
        dir.rename(QString{"___score_tmp___%1%2"}.arg(QDir::separator()).arg(entry), entry);
      }

      subdir.removeRecursively();
    }
  }

  if(!dir.exists("package.json"))
  {
    QFile f{dir.absoluteFilePath("package.json")};
    if(f.open(QIODevice::WriteOnly))
    {
      QJsonObject obj;
      obj["name"] = addon.name;
      obj["raw_name"] = addon.raw_name;
      obj["version"] = addon.version;
      obj["kind"] = addon.kind;
      obj["url"] = addon.url;
      obj["short"] = addon.shortDescription;
      obj["long"] = addon.longDescription;
      obj["size"] = addon.size;
      // TODO images
      obj["key"] = QString{score::uuids::toByteArray(addon.key.impl())};

      f.write(QJsonDocument{obj}.toJson());
    }
  }

  QMessageBox::information(
        m_widget,
        tr("Package downloaded"),
        tr("The package %1 has been successfully installed in the user library.")
        .arg(addon.name));

  auto& localPlugins
      = *static_cast<LocalPackagesModel*>(m_addonsOnSystem->model());

  localPlugins.registerAddon(dir.absolutePath());
  set_info();
}

void PluginSettingsView::on_packageInstallFailure(
      const Package& addon)
{
  reset_progress();
  QMessageBox::warning(
        m_widget,
        tr("Download failed"),
        tr("The package %1 could not be downloaded.").arg(addon.name));
}

void PluginSettingsView::set_info()
{
  m_storage->setText(QString::number(
                       storage.bytesAvailable() / 1024.0 / 1024.0 / 1024)
                     + " G\n"
                     + tr("available on volume"));
};

void PluginSettingsView::reset_progress()
{
  m_progress->setHidden(true);
  m_progress->setValue(0);
}

void PluginSettingsView::progress_from_bytes(qint64 bytesRecieved, qint64 bytesTotal)
{
  m_progress->setValue(((bytesRecieved / 1024.) / (bytesTotal / 1024.)) * 100);
}

}
