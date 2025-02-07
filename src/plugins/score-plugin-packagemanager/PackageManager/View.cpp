// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "View.hpp"

#include <score/application/GUIApplicationContext.hpp>
#include <score/widgets/MessageBox.hpp>
#include <score/widgets/SetIcons.hpp>

#include <core/application/ApplicationInterface.hpp>

#include <QApplication>
#include <QBuffer>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>

#include <PackageManager/FileDownloader.hpp>
#include <PackageManager/Presenter.hpp>

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
  if(idx != -1)
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
  storage = QStorageInfo(
      score::AppContext().settings<Library::Settings::Model>().getPackagesPath());

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

    tab_widget->addTab(local_widget, tr("Local packages"));
  }

  {
    auto remote_widget = new QWidget;
    auto remote_layout = new QVBoxLayout{remote_widget};
    remote_widget->setLayout(remote_layout);
    remote_layout->addWidget(m_remoteAddons);

    tab_widget->addTab(remote_widget, tr("Available packages"));
  }

  auto side_widget = new QWidget;
  auto vlay = new QVBoxLayout{side_widget};
  grid->addWidget(side_widget, 0, 1, 2, 1);

  auto categoryLabel = new QLabel("Select Category:");
  vlay->addWidget(categoryLabel);

  auto categoryComboBox = new QComboBox;
  categoryComboBox->addItem("All");
  categoryComboBox->addItem("Media");
  categoryComboBox->addItem("AI Models");
  vlay->addWidget(categoryComboBox);
  vlay->addSpacing(20);

  m_link->setToolTip(tr("Open external package link in default browser."));
  auto icon = makeIcons(
      QStringLiteral(":/icons/undock_on.png"), QStringLiteral(":/icons/undock_off.png"),
      QStringLiteral(":/icons/undock_off.png"));
  m_link->setIcon(icon);

  vlay->addWidget(m_link);

  vlay->addSpacing(20);

  vlay->addWidget(m_uninstall);
  m_install->setVisible(false);
  vlay->addWidget(m_install);
  vlay->addSpacing(20);

  vlay->addWidget(m_update);
  vlay->addWidget(m_updateAll);
  vlay->addStretch();

  set_info();
  vlay->addWidget(m_storage);

  for(QTableView* v : {m_addonsOnSystem, m_remoteAddons})
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

  connect(tab_widget, &QTabWidget::tabBarClicked, this, [this](int i) {
    if(i == 1) // Remote
    {
      m_uninstall->setVisible(false);
      m_install->setVisible(true);
      m_update->setVisible(false);
      m_updateAll->setVisible(false);

      RemotePackagesModel* model
          = static_cast<RemotePackagesModel*>(m_remoteAddons->model());
      model->clear();

      m_progress->setVisible(true);
      m_progress->setValue(0);

      refresh();
    }
    else // Local
    {
      m_uninstall->setVisible(true);
      m_install->setVisible(false);
      m_update->setVisible(true);
      m_updateAll->setVisible(true);
    }
  });

  connect(m_link, &QPushButton::pressed, this, &PluginSettingsView::openLink);

  connect(m_uninstall, &QPushButton::pressed, this, &PluginSettingsView::uninstall);

  connect(m_install, &QPushButton::pressed, this, &PluginSettingsView::install);

  connect(m_update, &QPushButton::pressed, this, &PluginSettingsView::update);

  connect(m_updateAll, &QPushButton::pressed, this, &PluginSettingsView::updateAll);

  connect(&mgr, &QNetworkAccessManager::finished, this, &PluginSettingsView::on_message);

  refresh();
}

QWidget* PluginSettingsView::getWidget()
{
  return m_widget;
}

void PluginSettingsView::firstTimeLibraryDownload()
{
  const auto& lib = score::GUIAppContext().settings<Library::Settings::Model>();
  const QString lib_folder = lib.getPackagesPath() + "/default";
  const QString lib_info = lib_folder + "/package.json";
  if(QFile file{lib_info}; !file.exists())
  {
    auto dl = score::question(
        qApp->activeWindow(), tr("Download the user library ?"),
        tr("The user library has not been found. \n"
           "Do you want to download it from the internet ? \n\n"
           "Note: you can always download it later from : \n"
           "https://github.com/ossia/score-user-library"));

    if(dl == QMessageBox::Yes)
    {
      zdl::download_and_extract(
          QUrl{"https://github.com/ossia/score-user-library/archive/master.zip"},
          lib.getPackagesPath(),
          [](const auto&) mutable {
        auto& lib = score::GUIAppContext().settings<Library::Settings::Model>();
        QDir packages_dir{lib.getPackagesPath()};
        packages_dir.rename("score-user-library-master", "default");

        lib.rescanLibrary();
          },
          [](qint64 bytesReceived, qint64 bytesTotal) {
        qDebug() << (((bytesReceived / 1024.) / (bytesTotal / 1024.)) * 100)
                 << "% downloaded";
      },
          [] {});
    }
  }
  else
  {
    checkAll();
  }
}

void PluginSettingsView::refresh()
{
  if(qEnvironmentVariableIsSet("SCORE_SANITIZE_SKIP_CHECKS"))
    return;
  QNetworkRequest rqst{
      QUrl("https://raw.githubusercontent.com/ossia/score-addons/refs/heads/"
           "add-ai-models/addons.json")};
  mgr.get(rqst);
}

void PluginSettingsView::handleAddonList(const QJsonObject& obj)
{
  m_progress->setVisible(true);
  auto arr = obj["addons"].toArray();
  m_addonsToRetrieve = arr.size();
  int delay = 0;
  for(QJsonValue elt : arr)
  {
    QTimer::singleShot(delay, this, [this, url = QUrl(elt.toString())] {
      QNetworkRequest rqst{url};
      mgr.get(rqst);
    });
    delay += 16;
  }
}

void PluginSettingsView::handleAddon(const QJsonObject& obj)
{
  RemotePackagesModel* model
      = static_cast<RemotePackagesModel*>(m_remoteAddons->model());

  if(m_addonsToRetrieve == std::ssize(model->m_vec))
    reset_progress();
  else
    m_progress->setValue(m_progress->value() + (100.0 / m_addonsToRetrieve));

  auto addon = Package::fromJson(obj);
  if(!addon)
    return;

  auto& add = *addon;

  // Load images
  if(!add.smallImagePath.isEmpty())
  {
    // c.f. https://wiki.qt.io/Download_Data_from_URL
    auto dl = new score::FileDownloader{QUrl{add.smallImagePath}};
    connect(dl, &score::FileDownloader::downloaded, this, [=](QByteArray arr) {
      model->updateAddon(
          add.key, [=](Package& add) { add.smallImage.loadFromData(arr); });

      dl->deleteLater();
    });
  }

  if(!add.largeImagePath.isEmpty())
  {
    // c.f. https://wiki.qt.io/Download_Data_from_URL
    auto dl = new score::FileDownloader{QUrl{add.largeImagePath}};
    connect(dl, &score::FileDownloader::downloaded, this, [=](QByteArray arr) {
      model->updateAddon(
          add.key, [=](Package& add) { add.largeImage.loadFromData(arr); });

      dl->deleteLater();
    });
  }

  model->addAddon(std::move(add));
}

void PluginSettingsView::on_message(QNetworkReply* rep)
{
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
  else
  {
    qDebug() << rep->request().url().toString() << ' ' << res;
    reset_progress();
  }
  rep->deleteLater();

  if(!m_firstTimeCheck)
  {
    m_firstTimeCheck = true;
    QTimer::singleShot(3000, this, &PluginSettingsView::firstTimeLibraryDownload);
  }
}

// the install button set to visible means we are browsing
PackagesModel* PluginSettingsView::getCurrentModel()
{
  if(m_install->isVisible())
    return static_cast<PackagesModel*>(m_remoteAddons->model());
  else
    return static_cast<PackagesModel*>(m_addonsOnSystem->model());
}

int PluginSettingsView::getCurrentRow(const QTableView* t = nullptr)
{
  QModelIndexList rows{};

  if(t)
    rows = t->selectionModel()->selectedRows(0);
  else
  {
    if(m_install->isVisible())
      rows = m_remoteAddons->selectionModel()->selectedRows(0);
    else
      rows = m_addonsOnSystem->selectionModel()->selectedRows(0);
  }

  if(rows.isEmpty())
    return -1;

  return rows.first().row();
}

Package PluginSettingsView::selectedPackage(const PackagesModel* model, int row)
{
  if(row == -1)
    return {};

  SCORE_ASSERT(int(model->addons().size()) > row);

  return model->addons().at(row);
}

void PluginSettingsView::openLink()
{
  const auto& addon = selectedPackage(getCurrentModel(), getCurrentRow());

  QDesktopServices::openUrl(addon.url);
}

void PluginSettingsView::install_package(const Package& addon)
{
  if(addon.kind == "addon" || addon.kind == "nodes")
    installAddon(addon);
  else if(addon.kind == "sdk")
    installSDK();
  else if(addon.kind == "media")
    installLibrary(addon);
  else if(addon.kind == "presets")
    installLibrary(addon);
}

void PluginSettingsView::install()
{
  const auto& addon = selectedPackage(
      static_cast<PackagesModel*>(m_remoteAddons->model()),
      getCurrentRow(m_remoteAddons));

  m_progress->setVisible(true);

  install_package(addon);
}

void PluginSettingsView::uninstall()
{
  const auto& addon = selectedPackage(
      static_cast<PackagesModel*>(m_addonsOnSystem->model()),
      getCurrentRow(m_addonsOnSystem));

  bool success{false};

  const auto& library{score::AppContext().settings<Library::Settings::Model>()};

  if(addon.kind == "addon" || addon.kind == "nodes" || addon.kind == "media")
  {
    success = QDir{library.getPackagesPath() + '/' + addon.raw_name}.removeRecursively();
  }
  else if(addon.kind == "sdk")
  {
    success = QDir{library.getSDKPath()}.removeRecursively();
  }

  if(success)
  {
    const auto& localPlugins
        = static_cast<LocalPackagesModel*>(m_addonsOnSystem->model());

    localPlugins->removeAddon(addon);
    set_info();
  }
}

void PluginSettingsView::update()
{
  auto local_model = static_cast<PackagesModel*>(m_addonsOnSystem->model());
  auto remote_model = static_cast<PackagesModel*>(m_remoteAddons->model());
  const auto& addon = selectedPackage(local_model, getCurrentRow(m_addonsOnSystem));

  auto key = addon.key;
  auto it = ossia::find_if(
      remote_model->addons(), [&](auto& pkg) { return pkg.key == addon.key; });
  if(it == remote_model->addons().end())
  {
    qDebug() << "Addon " << addon.name << "not found on the server!";
    return;
  }

  m_progress->setVisible(true);
  install_package(*it);
}

void PluginSettingsView::checkAll()
{
  auto local_model = static_cast<PackagesModel*>(m_addonsOnSystem->model());
  auto remote_model = static_cast<PackagesModel*>(m_remoteAddons->model());

  std::vector<Package*> to_update;
  for(auto& addon : local_model->addons())
  {
    auto key = addon.key;
    auto it = ossia::find_if(
        remote_model->addons(), [&](auto& pkg) { return pkg.key == addon.key; });
    if(it == remote_model->addons().end())
    {
      qDebug() << "Addon " << addon.name << "not found on the server!";
      continue;
    }

    if(it->version <= addon.version)
      continue;

    to_update.push_back(&*it);
  }

  if(!to_update.empty())
  {
    QString s = tr("Some installed packages are out-of-date: \n");
    for(auto pkg : to_update)
    {
      s += tr("- %1 (version %3)\n").arg(pkg->name).arg(pkg->version);
    }
    s += tr("Head to Settings > Packages to update them");
    score::information(qApp->activeWindow(), tr("Packages can be updated"), s);
  }
}

void PluginSettingsView::updateAll()
{
  auto local_model = static_cast<PackagesModel*>(m_addonsOnSystem->model());
  auto remote_model = static_cast<PackagesModel*>(m_remoteAddons->model());

  for(auto& addon : local_model->addons())
  {
    auto key = addon.key;
    auto it = ossia::find_if(
        remote_model->addons(), [&](auto& pkg) { return pkg.key == addon.key; });
    if(it == remote_model->addons().end())
    {
      qDebug() << "Addon " << addon.name << "not found on the server!";
      continue;
    }

    if(it->version <= addon.version)
      continue;

    m_progress->setVisible(true);
    install_package(*it);
  }
}

void PluginSettingsView::installAddon(const Package& addon)
{
  if(addon.files.empty())
  {
    reset_progress();
    return;
  }

  const QString& installPath
      = score::AppContext().settings<Library::Settings::Model>().getPackagesPath();
  for(auto f : addon.files)
    zdl::download_and_extract(
        f, QDir{installPath}.absolutePath(),
        [this, installPath, addon](const std::vector<QString>& res) {
      reset_progress();
      if(res.empty())
        return;
      // We want the extracted folder to have the name of the addon
      {
        QDir addons_dir{installPath};
        QFileInfo a_file(res[0]);
        auto d = a_file.dir();
        auto old_d = d;
        while(d.cdUp() && !d.isRoot())
        {
          if(d == addons_dir)
          {
            addons_dir.rename(old_d.dirName(), addon.raw_name);
            break;
          }
          old_d = d;
        }
      }

      QMessageBox::information(
          m_widget, tr("Addon downloaded"),
          tr("The addon %1 has been successfully installed in :\n"
             "%2\n\n"
             "It will be built and enabled shortly.\nCheck the message "
             "console for errors if nothing happens.")
              .arg(addon.name)
              .arg(QFileInfo(installPath).absoluteFilePath()));
    }, [this](qint64 received, qint64 total) { progress_from_bytes(received, total); },
        [this, addon] {
      reset_progress();
      QMessageBox::warning(
          m_widget, tr("Download failed"),
          tr("The package %1 could not be downloaded.").arg(addon.name));
    });
}

void PluginSettingsView::installSDK()
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
      score::AppContext().settings<Library::Settings::Model>().getSDKPath() + '/'
      + SCORE_TAG_NO_V};
  QDir{}.mkpath(sdk_path);

  const QUrl sdk_url
      = QString("https://github.com/ossia/score/releases/download/%1/%2-sdk.zip")
            .arg(SCORE_TAG)
            .arg(platform);

  zdl::download_and_extract(
      sdk_url, QFileInfo{sdk_path}.absoluteFilePath(),
      [this](const std::vector<QString>& res) {
    reset_progress();
    if(res.empty())
      return;

    QMessageBox::information(
        m_widget, tr("SDK downloaded"),
        tr("The SDK has been successfully installed in the user library."));

    set_info();
      },
      [this](qint64 received, qint64 total) { progress_from_bytes(received, total); },
      [this] {
    reset_progress();
    QMessageBox::warning(
        m_widget, tr("Download failed"), tr("The SDK could not be downloaded."));
  });
}

void PluginSettingsView::installLibrary(const Package& addon)
{
  const QString destination{
      score::AppContext().settings<Library::Settings::Model>().getPackagesPath() + "/"
      + addon.raw_name};

  if(QDir dest{destination}; dest.exists())
    dest.removeRecursively();

  QDir{}.mkpath(destination);

  for(auto f : addon.files)
    zdl::download_and_extract(
        f, QFileInfo{destination}.absoluteFilePath(),
        [this, addon, destination](const std::vector<QString>& res) {
      on_packageInstallSuccess(addon, destination, res);
    }, [this](qint64 received, qint64 total) { progress_from_bytes(received, total); },
        [this, addon] { on_packageInstallFailure(addon); });
}

void PluginSettingsView::on_packageInstallSuccess(
    const Package& addon, const QDir& destination, const std::vector<QString>& res)
{
  reset_progress();
  if(res.empty())
    return;

  // Often zip files contain a single, empty directory.
  // In that case, we move everything up a level to make the library cleaner.
  QDir dir{destination};
  auto files = dir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
  if(files.size() == 1)
  {
    auto child = files[0];
    QFileInfo info{dir.absoluteFilePath(child)};
    if(info.isDir())
    {
      dir.rename(child, "___score_tmp___");
      QDir subdir{dir.absoluteFilePath("___score_tmp___")};

      for(auto& entry :
          subdir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot))
      {
        dir.rename(
            QString{"___score_tmp___%1%2"}.arg(QDir::separator()).arg(entry), entry);
      }

      subdir.removeRecursively();
    }
  }

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
      m_widget, tr("Package downloaded"),
      tr("The package %1 has been successfully installed in the user library.")
          .arg(addon.name));

  auto& localPlugins = *static_cast<LocalPackagesModel*>(m_addonsOnSystem->model());

  localPlugins.registerAddon(dir.absolutePath());
  set_info();
}

void PluginSettingsView::on_packageInstallFailure(const Package& addon)
{
  reset_progress();
  QMessageBox::warning(
      m_widget, tr("Download failed"),
      tr("The package %1 could not be downloaded.").arg(addon.name));
}

void PluginSettingsView::set_info()
{
  m_storage->setText(
      QString::number(storage.bytesAvailable() / 1024.0 / 1024.0 / 1024) + " G\n"
      + tr("available on volume"));
};

void PluginSettingsView::reset_progress()
{
  m_progress->setHidden(true);
  m_progress->setValue(0);
}

void PluginSettingsView::progress_from_bytes(qint64 bytesReceived, qint64 bytesTotal)
{
  m_progress->setValue(((bytesReceived / 1024.) / (bytesTotal / 1024.)) * 100);
}

}
