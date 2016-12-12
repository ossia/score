#pragma once
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <Models/WidgetListModel.hpp>
#include <Models/CentralItemModel.hpp>
#include <WebSocketClient.hpp>
#include <Models/NodeModel.hpp>

namespace iscore {
class Settings;
class View;
class Presenter;
}  // namespace iscore

namespace RemoteUI
{

class RemoteApplication final :
    public QObject,
    public iscore::ApplicationInterface
{
public:
  RemoteApplication(int& argc, char** argv);
  ~RemoteApplication();

  const iscore::ApplicationContext& context() const override;
  const iscore::ApplicationComponents& components() const override;

  int exec();

  void enableListening(const Device::FullAddressSettings& a, GUIItem*);
  void disableListening(const Device::FullAddressSettings& a, GUIItem*);

private:
  void loadPlugins();
  // Base stuff.
  QGuiApplication m_app;
  QQmlApplicationEngine m_engine;

  iscore::ApplicationSettings m_applicationSettings;

  iscore::ApplicationComponentsData m_compData;
  iscore::ApplicationComponents m_components{m_compData};
  std::vector<std::unique_ptr<iscore::SettingsDelegateModel>> m_settings;
  iscore::ApplicationContext m_appContext;

  RemoteUI::WidgetListModel m_widgets{m_engine};
  RemoteUI::NodeModel m_nodes;
  WebSocketClient m_ws;

  RemoteUI::Context m_context;
  RemoteUI::CentralItemModel m_centralItemModel;

  std::unordered_map<State::Address, GUIItem*> m_listening;
};

}
