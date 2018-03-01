#pragma once
#include <PluginSettings/PluginItemModel.hpp>
#include <QGridLayout>
#include <QNetworkAccessManager>
#include <QProgressBar>
#include <QPushButton>
#include <QTabWidget>
#include <QTableView>
#include <QWidget>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
class QObject;

namespace PluginSettings
{
class PluginSettingsPresenter;
class PluginSettingsView : public score::GlobalSettingsView
{
  Q_OBJECT
public:
  PluginSettingsView();

  QTableView* localView()
  {
    return m_addonsOnSystem;
  }
  QTableView* remoteView()
  {
    return m_remoteAddons;
  }

  QPushButton& installButton() const
  {
    return *m_install;
  }

  QWidget* getWidget() override;

private:
  void handleAddonList(const QJsonObject&);
  void handleAddon(const QJsonObject&);

  QTabWidget* m_widget = new QTabWidget;

  QTableView* m_addonsOnSystem{new QTableView};

  QTableView* m_remoteAddons{new QTableView};
  QPushButton* m_refresh{new QPushButton{tr("Refresh")}};
  QPushButton* m_install{new QPushButton{tr("Install")}};

  QProgressBar* m_progress{new QProgressBar};
  QNetworkAccessManager mgr;

  int m_addonsToRetrieve = 0;
};
}
