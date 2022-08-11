#include <Process/ApplicationPlugin.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/ExecutionAction.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/OfflineAction/OfflineAction.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>

#include <LocalTree/ProcessComponent.hpp>
#include <Magnetism/MagnetismAdjuster.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>

#include <QGraphicsScene>

#include <score_lib_process.hpp>
#include <score_lib_process_commands_files.hpp>
namespace Process
{
DataflowManager::DataflowManager() { }

DataflowManager::~DataflowManager() { }

Dataflow::CableItem* DataflowManager::createCable(
    const Process::Cable& cable, const Process::Context& m_context,
    QGraphicsScene* scene)
{
  auto ptr = &cable;
  auto it = m_cableMap.find(ptr);
  if(it == m_cableMap.end())
  {
    auto item = new Dataflow::CableItem{*ptr, m_context, nullptr};
    m_cableMap.insert({ptr, item});
    if(!item->parentItem() && scene)
      scene->addItem(item);
    return item;
  }
  else if(it->second == nullptr)
  {
    auto item = new Dataflow::CableItem{*ptr, m_context, nullptr};
    it.value() = item;
    if(!item->parentItem() && scene)
      scene->addItem(item);
    return item;
  }

  return nullptr;
}
}
score_lib_process::score_lib_process()
{
  qRegisterMetaType<TimeVal>();
  qRegisterMetaType<Process::pan_weight>();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  qRegisterMetaTypeStreamOperators<TimeVal>();
  qRegisterMetaTypeStreamOperators<Process::pan_weight>();
#endif
}
score_lib_process::~score_lib_process() = default;

score::ApplicationPlugin*
score_lib_process::make_applicationPlugin(const score::ApplicationContext& app)
{
  return new Process::ApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_lib_process::factoryFamilies()
{
  return make_ptr_vector<
      score::InterfaceListBase, Process::ProcessFactoryList, Process::PortFactoryList,
      Process::LayerFactoryList, Process::ProcessFactoryList,
      Process::ProcessDropHandlerList, Process::MagnetismAdjuster,
      Process::OfflineActionList, Execution::ExecutionActionList,
      LocalTree::ProcessComponentFactoryList>();
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_lib_process::make_commands()
{
  using namespace Process;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_lib_process_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_lib_process)
