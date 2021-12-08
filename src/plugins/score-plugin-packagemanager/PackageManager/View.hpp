#pragma once
#include <score/application/ApplicationContext.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
#include <Library/LibrarySettings.hpp>

#include <QGridLayout>
#include <QNetworkAccessManager>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QTableView>
#include <QWidget>
#include <QStorageInfo>
#include <QLabel>

#include <PackageManager/PluginItemModel.hpp>

#include <verdigris>
class QObject;

namespace PM
{
class PluginSettingsPresenter;
class PluginSettingsView : public score::GlobalSettingsView
{
  W_OBJECT(PluginSettingsView)
public:
  PluginSettingsView();

  QTableView* localView() { return m_addonsOnSystem; }
  QTableView* remoteView() { return m_remoteAddons; }

  QPushButton& installButton() const { return *m_install; }

  QWidget* getWidget() override;

private:
  void handleAddonList(const QJsonObject&);
  void handleAddon(const QJsonObject&);

  PackagesModel* getCurrentModel();
  int getCurrentRow(const QTableView* t);
  Package selectedPackage(const PackagesModel* model, int row);

  void installAddon(const Package& addon);
  void installLibrary(const Package& addon);
  void installSDK(const Package& addon);

  void openLink();
  void install();
  void uninstall();
  void on_message(QNetworkReply* rep);

  void on_packageInstallSuccess(const Package& addon, const QDir& destination, const std::vector<QString>& res);
  void on_packageInstallFailure(const Package& addon);

  void set_info();
  void reset_progress();
  void progress_from_bytes(qint64 bytesRecieved, qint64 bytesTotal);

  QWidget* m_widget{new QWidget};

  QTableView* m_addonsOnSystem{new QTableView};
  QTableView* m_remoteAddons{new QTableView};

  QPushButton* m_link{new QPushButton{tr("Open link")}};
  QPushButton* m_install{new QPushButton{tr("Install")}};
  QPushButton* m_uninstall{new QPushButton{tr("Uninstall")}};

  QProgressBar* m_progress{new QProgressBar};
  QNetworkAccessManager mgr;
  int m_addonsToRetrieve = 0;

  QStorageInfo storage;
  QLabel* m_storage{new QLabel};
};
}
