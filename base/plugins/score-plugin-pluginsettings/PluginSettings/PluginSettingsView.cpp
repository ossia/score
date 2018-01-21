// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QGridLayout>

#include "PluginSettingsPresenter.hpp"
#include "PluginSettingsView.hpp"
#include <PluginSettings/FileDownloader.hpp>
#include <QHeaderView>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <QNetworkRequest>
#include <QTemporaryFile>

#include <QBuffer>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>

#include <miniz.c>

namespace PluginSettings
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

std::vector<QString> unzip(const QString& zipFile,
                               const QString& path)
{
    std::vector<QString> files;
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    auto status = mz_zip_reader_init_file(&zip_archive, zipFile.toUtf8(), 0);
    if (!status) return files;
    int fileCount = (int)mz_zip_reader_get_num_files(&zip_archive);
    if (fileCount == 0)
    {
        mz_zip_reader_end(&zip_archive);
        return files;
    }
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&zip_archive, 0, &file_stat))
    {
        mz_zip_reader_end(&zip_archive);
        return files;
    }

    // Get root folder
    QString lastDir = "";
    QString base = slash_path(get_path(file_stat.m_filename)); // path delim on end

    // Get and print information about each file in the archive.
    for (int i = 0; i < fileCount; i++)
    {
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat)) continue;
        if (mz_zip_reader_is_file_a_directory(&zip_archive, i)) continue; // skip directories for now
        QString fileName = relative_path(base, file_stat.m_filename); // make path relative
        QString destFile = combine_path(path, fileName); // make full dest path
        auto newDir = get_path(fileName); // get the file's path
        if (newDir != lastDir)
        {
            if (!make_folder(combine_path(path, newDir))) // creates the directory
            {
                return files;
            }
        }

        if (mz_zip_reader_extract_to_file(&zip_archive, i, destFile.toUtf8(), 0))
        {
            files.emplace_back(destFile);
        }
    }

    // Close the archive, freeing any resources it was using
    mz_zip_reader_end(&zip_archive);
    return files;
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
      &mgr, &QNetworkAccessManager::finished, this,
      [this](QNetworkReply* rep) {
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

        rep->deleteLater();
      });

  connect(m_refresh, &QPushButton::pressed, this, [this]() {

    RemotePluginItemModel* model
        = static_cast<RemotePluginItemModel*>(m_remoteAddons->model());
    model->clear();

    m_progress->setVisible(true);

    QNetworkRequest rqst{
        QUrl("https://raw.githubusercontent.com/OSSIA/score-addons/master/"
             "addons.json")};
    mgr.get(rqst);

  });
#if !defined(_MSC_VER)

  connect(m_install, &QPushButton::pressed, this, [this]() {

    RemotePluginItemModel& remotePlugins
        = *static_cast<RemotePluginItemModel*>(m_remoteAddons->model());

    auto num = m_remoteAddons->selectionModel()->selectedRows(0).first().row();
    RemoteAddon& addon = remotePlugins.addons().at(num);

    auto it = addon.architectures.find(score::addonArchitecture());
    if (it == addon.architectures.end())
      return;
    if (it->second == QUrl{})
      return;

    m_progress->setVisible(true);
    auto dl = new score::FileDownloader{it->second};
    connect(
        dl, &score::FileDownloader::downloaded, this,
        [&, dl, addon](QByteArray arr) {

          QTemporaryFile f;
          f.open();
          f.setAutoRemove(true);
          f.write(arr);

          QFileInfo fi{f};

#if defined(SCORE_DEPLOYMENT_BUILD)
          auto docs = QStandardPaths::standardLocations(
                          QStandardPaths::DocumentsLocation)
                          .first();
          auto dirname = docs + "/score/plugins/";
#else
            auto dirname = "addons";
#endif

          auto res = zip_helper::unzip(fi.absoluteFilePath(), dirname);

          dl->deleteLater();
          m_progress->setHidden(true);
          if (res.size() > 0)
          {
            QMessageBox::information(
                m_widget,
                tr("Addon downloaded"),
                tr("The addon %1 has been succesfully installed in :\n"
                   "%2\n\n"
                   "Please restart score to enable it.")
                    .arg(addon.name)
                    .arg(QFileInfo(dirname).absoluteFilePath()));
          }
          else
          {
            QMessageBox::warning(
                m_widget,
                tr("Download failed"),
                tr("The addon %1 could not be downloaded."));
          }
        });
  });
#endif
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
  using Funmap = std::map<QString, std::function<void(QJsonValue)>>;

  RemoteAddon add;
  QString smallImage,
      largeImage; // thank you, Microsh**, for #define small char
  const Funmap funmap{
      {"name", [&](QJsonValue v) { add.name = v.toString(); }},
      {"version", [&](QJsonValue v) { add.version = v.toString(); }},
      {"url", [&](QJsonValue v) { add.latestVersionAddress = v.toString(); }},
      {"short", [&](QJsonValue v) { add.shortDescription = v.toString(); }},
      {"long", [&](QJsonValue v) { add.longDescription = v.toString(); }},
      {"small", [&](QJsonValue v) { smallImage = v.toString(); }},
      {"large", [&](QJsonValue v) { largeImage = v.toString(); }},
      {"key", [&](QJsonValue v) {
         add.key = UuidKey<score::Addon>::fromString(v.toString());
       }}};

  // Add metadata keys
  for (const auto& k : obj.keys())
  {
    auto it = funmap.find(k);
    if (it != funmap.end())
    {
      it->second(obj[k]);
    }
  }

  if (add.key.impl().is_nil() || add.name.isEmpty())
  {
    return;
  }

  // Add architecture keys
  add.architectures = addonArchitectures();
  for (auto& k : add.architectures)
  {
    auto it = obj.constFind(k.first);
    if (it != obj.constEnd())
    {
      k.second = (*it).toString();
    }
  }

  // Load images
  RemotePluginItemModel* model
      = static_cast<RemotePluginItemModel*>(m_remoteAddons->model());
  if (!smallImage.isEmpty())
  {
    // c.f. https://wiki.qt.io/Download_Data_from_URL
    auto dl = new score::FileDownloader{QUrl{smallImage}};
    connect(
        dl, &score::FileDownloader::downloaded, this, [=](QByteArray arr) {
          model->updateAddon(add.key, [=](RemoteAddon& add) {
            add.smallImage.loadFromData(arr);
          });

          dl->deleteLater();
        });
  }

  if (!largeImage.isEmpty())
  {
    // c.f. https://wiki.qt.io/Download_Data_from_URL
    auto dl = new score::FileDownloader{QUrl{largeImage}};
    connect(
        dl, &score::FileDownloader::downloaded, this, [=](QByteArray arr) {
          model->updateAddon(add.key, [=](RemoteAddon& add) {
            add.largeImage.loadFromData(arr);
          });

          dl->deleteLater();
        });
  }

  model->addAddon(std::move(add));
}
}
