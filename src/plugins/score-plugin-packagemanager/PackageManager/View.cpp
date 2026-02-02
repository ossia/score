// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "View.hpp"

#include <score/application/GUIApplicationContext.hpp>
#include <score/widgets/MessageBox.hpp>

#include <core/application/ApplicationInterface.hpp>

#include <QApplication>
#include <QBuffer>
#include <QDesktopServices>
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

void PluginSettingsView::handleAddonList(const QJsonObject& obj)
{
  m_progress->setVisible(true);
  auto arr = obj["addons"].toArray();
  m_addonsToRetrieve = arr.size();
  int delay = 0;
  for(QJsonValue elt : arr)
  {
    QTimer::singleShot(
        delay, this, [this, url = QUrl(elt.toString())] { requestInformation(url); });
    delay += 16;
  }
}

void PluginSettingsView::handleAddon(const QJsonObject& obj)
{
  RemotePackagesModel* model
      = static_cast<RemotePackagesModel*>(m_remoteAddons->model());

  if(m_addonsToRetrieve == (std::ssize(model->m_vec) + 1))
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
  else
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
