#include <QCoreApplication>
#include <core/application/ApplicationSettings.hpp>
#include <core/application/SafeQApplication.hpp>
#include <core/document/DocumentBuilder.hpp>
#include <core/document/DocumentModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentFactory.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <Engine/Executor/Settings/ExecutorModel.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/application/ApplicationInterface.hpp>
#include <core/application/ApplicationRegistrar.hpp>

#include <core/plugin/PluginManager.hpp>
#include <core/plugin/PluginDependencyGraph.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <iscore/model/ComponentSerialization.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <Process/ProcessList.hpp>
#include <Engine/Executor/ConstraintComponent.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>

#include <ossia/editor/scenario/time_constraint.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <QPluginLoader>
#include <QJsonDocument>

#if defined(ISCORE_STATIC_PLUGINS)
  #include <iscore_static_plugins.hpp>
#endif
struct PlayerApp : public iscore::ApplicationInterface
{
public:
  PlayerApp(iscore::ApplicationContext& x):
    ctx{x}
  {
    m_instance = this;
  }

  iscore::ApplicationContext& ctx;

  const iscore::ApplicationContext& context() const override
  {
    return ctx;
  }

  const iscore::ApplicationComponents& components() const override
  {
    return ctx.components;
  }
};
void loadPlugins(
    iscore::ApplicationRegistrar& registrar, const iscore::ApplicationContext& context)
{
  using namespace iscore;
  using namespace iscore::PluginLoader;
  // Here, the plug-ins that are effectively loaded.
  std::vector<iscore::Addon> availablePlugins;

  // Load static plug-ins
  for (QObject* plugin : QPluginLoader::staticInstances())
  {
    if (auto iscore_plug = dynamic_cast<iscore::Plugin_QtInterface*>(plugin))
    {
      iscore::Addon addon;
      addon.corePlugin = true;
      addon.plugin = iscore_plug;
      addon.key = iscore_plug->key();
      addon.corePlugin = true;
      availablePlugins.push_back(std::move(addon));
    }
  }

  loadPluginsInAllFolders(availablePlugins);
  loadAddonsInAllFolders(availablePlugins);

  registrar.registerAddons(availablePlugins);

  for (const iscore::Addon& addon : availablePlugins)
  {
    auto facfam_interface
        = dynamic_cast<FactoryList_QtInterface*>(addon.plugin);

    if (facfam_interface)
    {
      for (auto&& elt : facfam_interface->factoryFamilies())
      {
        registrar.registerFactory(std::move(elt));
      }
    }
  }

  PluginDependencyGraph graph{availablePlugins};
  const auto& add = graph.sortedAddons();
  if(!add.empty())
  {

    for(const iscore::Addon& addon : availablePlugins)
    {
      auto ctrl_plugin
          = dynamic_cast<ApplicationPlugin_QtInterface*>(addon.plugin);
      if (ctrl_plugin)
      {
        if(auto plug = ctrl_plugin->make_applicationPlugin(context))
          registrar.registerApplicationPlugin(plug);
      }
    }

    for (const iscore::Addon& addon : availablePlugins)
    {
      auto commands_plugin
          = dynamic_cast<CommandFactory_QtInterface*>(addon.plugin);
      if (commands_plugin)
      {
        registrar.registerCommands(commands_plugin->make_commands());
      }

      auto factories_plugin
          = dynamic_cast<FactoryInterface_QtInterface*>(addon.plugin);
      if (factories_plugin)
      {
        for (auto& factory_family : registrar.components().factories)
        {
          for (auto&& new_factory :
               factories_plugin->factories(context, factory_family.first))
          {
            factory_family.second->insert(std::move(new_factory));
          }
        }
      }
    }

  }
}
int main(int argc, char** argv)
{
  using namespace iscore;

  // Load core application and plug-ins
  SafeQApplication app{argc, argv};

  iscore::ApplicationSettings glob_settings;
  iscore::ApplicationComponentsData data;

  ApplicationComponents comps{data};
  DocumentList docs;
  std::vector<std::unique_ptr<iscore::SettingsDelegateModel>> setgs;
  iscore::ApplicationContext ctx{glob_settings, comps, docs, std::move(setgs)};

  PlayerApp x{ctx};

  iscore::ApplicationRegistrar reg(data);
  reg.registerFactory(std::make_unique<iscore::DocumentDelegateList>());
  reg.registerFactory(std::make_unique<iscore::ValidityCheckerList>());
  reg.registerFactory(std::make_unique<iscore::SerializableComponentFactoryList>());
  reg.registerFactory(std::make_unique<iscore::DocumentPluginFactoryList>());
  reg.registerFactory(std::make_unique<iscore::SettingsDelegateFactoryList>());

  loadPlugins(reg, ctx);

  QSettings s;
  for (auto& plugin :
       ctx.interfaces<iscore::SettingsDelegateFactoryList>())
  {
    setgs.push_back(plugin.makeModel(s, ctx));
  }

  // Load a document
  QFile f("/home/jcelerier/i-score/Tests/testdata/execution.scorejson");
  f.open(QIODevice::ReadOnly);

  auto json = QJsonDocument::fromJson(f.readAll()).object();
  Scenario::ScenarioDocumentFactory fac;
  Document doc{json, fac, qApp};

  docs.documents().push_back(&doc);

  auto& doc_model = safe_cast<Scenario::ScenarioDocumentModel&>(doc.model().modelDelegate());
  auto& root_cst = doc_model.baseConstraint();
  auto exec = new Engine::Execution::DocumentPlugin{doc.context(), Id<DocumentPlugin>{998}, nullptr};
  auto lt = new Engine::LocalTree::DocumentPlugin{doc.context(), Id<DocumentPlugin>{999}, nullptr};
  lt->init();
  doc.model().addPluginModel(lt);
  doc.model().addPluginModel(exec);

  // Setup execution
  {
    exec->reload(root_cst);
    auto& exec_settings = ctx.settings<Engine::Execution::Settings::Model>();
    auto clock = exec_settings.makeClock(exec->context());

    clock->play(TimeVal::zero());
    while(exec->context().scenario.baseConstraint().OSSIAConstraint()->running())
      ;
  }

  exec->clear();
  return 0;
}
