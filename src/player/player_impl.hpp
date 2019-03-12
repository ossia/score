
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/ComponentSerialization.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <score/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <score/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <score/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/tools/std/Optional.hpp>

#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/application/SafeQApplication.hpp>
#include <core/document/DocumentBuilder.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/plugin/PluginDependencyGraph.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia-qt/device/qml_device.hpp>
#include <ossia/context.hpp>
#include <ossia/editor/scenario/time_interval.hpp>

#include <QCoreApplication>

#include <Execution/DocumentPlugin.hpp>
#include <Execution/IntervalComponent.hpp>
#include <Execution/Settings/ExecutorModel.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <score_player_export.h>
namespace Network
{
class NetworkDocumentPlugin;
}
namespace score
{
class SCORE_PLAYER_EXPORT PlayerImpl : public QObject,
                                       public ApplicationInterface
{
  Q_OBJECT

public:
  PlayerImpl();

  PlayerImpl(bool);

  ~PlayerImpl();

  void init();
  void registerPluginPath(std::string);
  void closeDocument();

  void exec();
  void close();

  void loadFile(QString file);
  void loadArray(QByteArray network);

  void registerDevice(ossia::net::device_base*);
  void releaseDevice(ossia::net::device_base*);
  void setPort(int);

  void prepare_play();
  void do_play();

  void play()
  {
    prepare_play();
    do_play();
  }

  void stop();

  void loadPlugins(
      ApplicationRegistrar& registrar, const ApplicationContext& context);

public:
  void sig_play();
  void sig_stop();
  void sig_close();
  void sig_loadFile(QString);
  void sig_setPort(int);
  void sig_registerDevice(ossia::net::device_base*);

private:
  void setupLoadedDocument();
  const ApplicationContext& context() const override;
  const ApplicationComponents& components() const override;

  int argc = 1;
  char** argv{};

  // Load core application and plug-ins
  std::unique_ptr<QCoreApplication> m_app{};

  // Application-specific
  std::string m_pluginPath;
  ApplicationSettings m_globSettings;
  ApplicationComponentsData m_compData;

  ApplicationComponents m_components{m_compData};
  DocumentList m_documents;
  std::vector<std::unique_ptr<SettingsDelegateModel>> m_settings;
  ApplicationContext m_appContext{m_globSettings, m_components, m_documents,
                                  m_settings};

  // Document-specific
  std::unique_ptr<Document> m_currentDocument;
  Explorer::DeviceDocumentPlugin* m_devicesPlugin{};
  Execution::DocumentPlugin* m_execPlugin{};
  LocalTree::DocumentPlugin* m_localTreePlugin{};
  Network::NetworkDocumentPlugin* m_networkPlugin{};
  std::unique_ptr<Execution::Clock> m_clock;

  std::vector<ossia::net::device_base*> m_ownedDevices;
};
}
