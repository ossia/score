// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Model.hpp"

#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/widgets/MessageBox.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSet>
#include <QSettings>
#include <QStandardItemModel>
#include <QStringList>
#include <QVariant>
#include <qmessagebox.h>

#include <PackageManager/FileDownloader.hpp>

#include <score_git_info.hpp>
#include <wobjectimpl.h>
#include <zipdownloader.hpp>
W_OBJECT_IMPL(PM::PluginSettingsModel)
namespace PM
{
PluginSettingsModel::PluginSettingsModel(
    QSettings& set, const score::ApplicationContext& ctx)
    : score::SettingsDelegateModel{}
    , localPlugins{ctx}
    , remoteSelection{&remotePlugins}
{
  connect(
      &mgr, &QNetworkAccessManager::finished, this, &PluginSettingsModel::on_message);
}

PluginSettingsModel::~PluginSettingsModel() { }

void PluginSettingsModel::refresh()
{
  if(qEnvironmentVariableIsSet("SCORE_SANITIZE_SKIP_CHECKS"))
    return;
  QNetworkRequest rqst{QUrl(
      "https://raw.githubusercontent.com/ossia/score-packages/refs/heads/"
      "master/addons.json")};
  mgr.get(rqst);
}

void PluginSettingsModel::handleAddonList(const QJsonObject& obj)
{
  show_progress();
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

void PluginSettingsModel::handleAddon(const QJsonObject& obj)
{
  RemotePackagesModel* model = &remotePlugins;

  if(m_addonsToRetrieve == (std::ssize(model->m_vec) + 1) || m_addonsToRetrieve <= 0)
    reset_progress();
  else
    update_progress(double(std::ssize(model->m_vec) + 1) / m_addonsToRetrieve);

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

void PluginSettingsModel::on_message(QNetworkReply* rep)
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
    QTimer::singleShot(3000, this, &PluginSettingsModel::firstTimeLibraryDownload);
  }
}

void PluginSettingsModel::firstTimeLibraryDownload()
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
          lib.getPackagesPath(), [](const auto&) mutable {
        auto& lib = score::GUIAppContext().settings<Library::Settings::Model>();
        QDir packages_dir{lib.getPackagesPath()};
        packages_dir.rename("score-user-library-master", "default");

        lib.rescanLibrary();
      }, [](qint64 bytesReceived, qint64 bytesTotal) {
        qDebug() << (((bytesReceived / 1024.) / (bytesTotal / 1024.)) * 100)
                 << "% downloaded";
      }, [] {});
    }
  }
  else
  {
    checkAll();
  }
}

void PluginSettingsModel::checkAll()
{
  auto local_model = &localPlugins;
  auto remote_model = &remotePlugins;

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

void PluginSettingsModel::requestInformation(QUrl url)
{
  QNetworkRequest rqst{url};
  mgr.get(rqst);
}

void PluginSettingsModel::installAddon(const Package& addon)
{
  if(addon.files.empty())
  {
    reset_progress();
    return;
  }

  const auto& lib = score::AppContext().settings<Library::Settings::Model>();
  const QString& installPath
      = addon.kind == "support" ? lib.getSupportPath() : lib.getPackagesPath();

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

      information(
          tr("Addon downloaded"),
          tr("The addon %1 has been successfully installed in :\n"
             "%2\n\n"
             "It will be built and enabled shortly.\nCheck the message "
             "console for errors if nothing happens.")
              .arg(addon.name)
              .arg(QFileInfo(installPath).absoluteFilePath()));
    }, [this](qint64 received, qint64 total) { progress_from_bytes(received, total); },
        [this, addon] {
      reset_progress();
      warning(
          tr("Download failed"),
          tr("The package %1 could not be downloaded.").arg(addon.name));
    });
}

void PluginSettingsModel::installSDK()
{
  auto platform = score::addonArchitecture();

  const QString sdk_path{
      score::AppContext().settings<Library::Settings::Model>().getSDKPath() + '/'
      + SCORE_TAG_NO_V};
  QDir{}.mkpath(sdk_path);

  const QUrl sdk_url
      = QString("https://github.com/ossia/score/releases/download/%1/sdk-%2.zip")
            .arg(SCORE_TAG)
            .arg(platform);

  zdl::download_and_extract(
      sdk_url, QFileInfo{sdk_path}.absoluteFilePath(),
      [this](const std::vector<QString>& res) {
    reset_progress();
    if(res.empty())
      return;
    information(
        tr("SDK downloaded"),
        tr("The SDK has been successfully installed in the user library."));

    set_info();
  }, [this](qint64 received, qint64 total) { progress_from_bytes(received, total); },
      [this] {
    reset_progress();
    warning(tr("Download failed"), tr("The SDK could not be downloaded."));
  });
}

void PluginSettingsModel::installLibrary(const Package& addon)
{
  const auto& lib = score::AppContext().settings<Library::Settings::Model>();
  const QString& installPath
      = addon.kind == "support" ? lib.getSupportPath() : lib.getPackagesPath();
  const QString destination{installPath + "/" + addon.raw_name};

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

void PluginSettingsModel::on_packageInstallSuccess(
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

  information(
      tr("Package downloaded"),
      tr("The package %1 has been successfully installed in the user library.")
          .arg(addon.name));

  localPlugins.registerAddon(dir.absolutePath());
  set_info();
}

void PluginSettingsModel::on_packageInstallFailure(const Package& addon)
{
  reset_progress();
  warning(
      tr("Download failed"),
      tr("The package %1 could not be downloaded.").arg(addon.name));
}
}
