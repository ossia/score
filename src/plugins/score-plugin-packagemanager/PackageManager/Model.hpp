#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <QItemSelectionModel>
#include <QNetworkAccessManager>

#include <PackageManager/PluginItemModel.hpp>

#include <score_plugin_packagemanager_export.h>

namespace PM
{
class BlacklistCommand;

class SCORE_PLUGIN_PACKAGEMANAGER_EXPORT PluginSettingsModel
    : public score::SettingsDelegateModel
{
  W_OBJECT(PluginSettingsModel)

public:
  PluginSettingsModel(QSettings& set, const score::ApplicationContext& ctx);
  ~PluginSettingsModel();

  void refresh();
  void requestInformation(QUrl url);

  void installAddon(const Package& addon);
  void installLibrary(const Package& addon);
  void installSDK();

  void handleAddonList(const QJsonObject&);
  void handleAddon(const QJsonObject&);
  void on_message(QNetworkReply* rep);
  void firstTimeLibraryDownload();
  void checkAll();

  void show_progress() W_SIGNAL(show_progress);
  void update_progress(double v) W_SIGNAL(update_progress, v);
  void reset_progress() W_SIGNAL(reset_progress);
  void set_info() W_SIGNAL(set_info);
  void progress_from_bytes(qint64 received, qint64 total)
      W_SIGNAL(progress_from_bytes, received, total);
  void information(QString title, QString description)
      W_SIGNAL(information, title, description);
  void warning(QString title, QString description) W_SIGNAL(warning, title, description);

  void on_packageInstallSuccess(
      const Package& addon, const QDir& destination, const std::vector<QString>& res);
  void on_packageInstallFailure(const Package& addon);

  LocalPackagesModel localPlugins;
  RemotePackagesModel remotePlugins;
  QItemSelectionModel remoteSelection;

  QNetworkAccessManager mgr;
  int m_addonsToRetrieve = 0;
  bool m_firstTimeCheck{false};
};
}
