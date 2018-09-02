#pragma once
#include <Models/CentralItemModel.hpp>
#include <Models/NodeModel.hpp>
#include <Models/WidgetListModel.hpp>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <WebSocketClient.hpp>
#include <ZeroconfListener.hpp>
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <ossia/network/generic/generic_device.hpp>

namespace score
{
class Settings;
class Presenter;
} // namespace score

namespace RemoteUI
{

class RemoteApplication final
    : public QObject
    , public score::ApplicationInterface
{
public:
  RemoteApplication(int& argc, char** argv);
  ~RemoteApplication();

  const score::ApplicationContext& context() const override;
  const score::ApplicationComponents& components() const override;

  int exec();

private:
  void loadPlugins();
  // Base stuff.
  QGuiApplication m_app;
  QQmlApplicationEngine m_engine;

  score::ApplicationSettings m_applicationSettings;

  score::ApplicationComponentsData m_compData;
  score::ApplicationComponents m_components{m_compData};
  std::vector<std::unique_ptr<score::SettingsDelegateModel>> m_settings;
  score::DocumentList m_docs;
  score::ApplicationContext m_appContext;

  RemoteUI::WidgetListModel m_widgets{m_engine};
  RemoteUI::NodeModel m_nodes;

  RemoteUI::Context m_context;
  RemoteUI::CentralItemModel m_centralItemModel;
  std::vector<std::unique_ptr<ossia::net::generic_device>> m_clients;
  ZeroconfListener m_zeroconf;
};
}
