#include "score_plugin_media.hpp"

#include <Library/JSONLibrary/ProcessesItemModel.hpp>
#include <Library/LibraryInterface.hpp>
#include <Media/ApplicationPlugin.hpp>
#include <Media/Effect/EffectExecutor.hpp>
#include <Media/Effect/EffectProcessFactory.hpp>
#include <Media/Effect/Inspector/EffectInspector.hpp>
#include <Media/Effect/Settings/Factory.hpp>
#include <Media/Inspector/Factory.hpp>
#include <Media/Merger/Executor.hpp>
#include <Media/Merger/Factory.hpp>
#include <Media/Merger/Inspector.hpp>
#include <Media/Sound/Drop/SoundDrop.hpp>
#include <Media/Sound/SoundComponent.hpp>
#include <Media/Sound/SoundFactory.hpp>
#include <Media/Step/Executor.hpp>
#include <Media/Step/Factory.hpp>
#include <Media/Step/Inspector.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>

#include <QAction>

#if defined(LILV_SHARED)
#include <Media/Effect/LV2/LV2EffectModel.hpp>
#include <Media/Effect/LV2/LV2Window.hpp>
#endif
#if defined(HAS_VST2)
#include <Media/Effect/VST/VSTControl.hpp>
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <Media/Effect/VST/VSTExecutor.hpp>
#include <Media/Effect/VST/VSTWidgets.hpp>
#endif
#if defined(HAS_FAUST)
#include <Media/Effect/Faust/FaustEffectModel.hpp>
#endif
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/FactorySetup.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <score_plugin_media_commands_files.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::Step::View)
W_OBJECT_IMPL(Media::Effect::EffectTitleItem)

#if defined(HAS_VST2)
namespace Media::VST
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("6a13c3cc-bca7-44d6-a0ef-644e99204460")
  void setup(
      Library::ProcessesItemModel& model,
      const score::GUIApplicationContext& ctx) override
  {
    const auto& key = VSTEffectFactory{}.concreteKey();
    QModelIndex node = model.find(key);
    if (node == QModelIndex{})
    {
      return;
    }
    auto& parent
        = *reinterpret_cast<Library::ProcessNode*>(node.internalPointer());

    auto& fx = parent.emplace_back(Library::ProcessData{"Effects", QIcon{}, {}, {}}, &parent);
    auto& inst = parent.emplace_back(Library::ProcessData{"Instruments", QIcon{}, {}, {}}, &parent);
    auto& plug = ctx.applicationPlugin<Media::ApplicationPlugin>();
    for (const auto& vst : plug.vst_infos)
    {
      if (vst.isValid)
      {
        QJsonObject obj;
        obj["Type"] = "Process";
        obj["uuid"] = toJsonValue(key.impl());
        obj["Data"] = QString::number(vst.uniqueID);

        if(vst.isSynth)
        {
          inst.emplace_back(
              Library::ProcessData{vst.displayName, QIcon{}, obj, key}, &inst);
        }
        else
        {
          fx.emplace_back(
              Library::ProcessData{vst.displayName, QIcon{}, obj, key}, &fx);
        }
      }
    }
  }
};
}
#endif

#if defined(LILV_SHARED)
namespace Media::LV2
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("570f0b92-a091-47ff-a5c3-a585e07df2bf")
  void setup(
      Library::ProcessesItemModel& model,
      const score::GUIApplicationContext& ctx) override
  {
    const auto& key = LV2EffectFactory{}.concreteKey();
    QModelIndex node = model.find(key);
    if (node == QModelIndex{})
    {
      return;
    }
    auto& parent
        = *reinterpret_cast<Library::ProcessNode*>(node.internalPointer());

    auto& plug = ctx.applicationPlugin<Media::ApplicationPlugin>();
    auto& world = plug.lilv;

    auto plugs = world.get_all_plugins();

    std::map<QString, QVector<QString>> categories;

    auto it = plugs.begin();
    while (!plugs.is_end(it))
    {
      auto plug = plugs.get(it);
      const auto class_name = plug.get_class().get_label().as_string();
      const auto plug_name = plug.get_name().as_string();
      categories[class_name].push_back(plug_name);
      it = plugs.next(it);
    }

    for (auto& category : categories)
    {
      auto& cat = parent.emplace_back(
          Library::ProcessData{category.first, QIcon{}, {}, {}}, &parent);
      for (auto& plug : category.second)
      {
        QJsonObject obj;
        obj["Type"] = "Process";
        obj["uuid"] = toJsonValue(key.impl());
        obj["Data"] = plug;
        cat.emplace_back(Library::ProcessData{plug, QIcon{}, obj, key}, &cat);
      }
    }
  }
};
}
#endif

score_plugin_media::score_plugin_media()
{
}

score_plugin_media::~score_plugin_media()
{
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_media::make_commands()
{
  using namespace Media;
  using namespace Media::VST;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Media::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_media_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

score::ApplicationPlugin* score_plugin_media::make_applicationPlugin(
    const score::ApplicationContext& app)
{
  return new Media::ApplicationPlugin{app};
}

score::GUIApplicationPlugin* score_plugin_media::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  return new Media::GUIApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_media::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Media::Sound::ProcessFactory,
         Media::Effect::ProcessFactory, Media::Step::ProcessFactory,
         Media::Merger::ProcessFactory
#if defined(HAS_FAUST)
         ,
         Media::Faust::FaustEffectFactory
#endif
#if defined(LILV_SHARED)
         ,
         Media::LV2::LV2EffectFactory
#endif
#if defined(HAS_VST2)
         ,
         Media::VST::VSTEffectFactory
#endif
         >,
      FW<Inspector::InspectorWidgetFactory, Media::Sound::InspectorFactory,
         Media::Effect::InspectorFactory
#if defined(HAS_FAUST)
         ,
         Media::Faust::InspectorFactory
#endif
         ,
         Media::Step::InspectorFactory, Media::Merger::InspectorFactory>,
      FW<Process::LayerFactory, Media::Sound::LayerFactory,
         Media::Effect::LayerFactory, Media::Step::LayerFactory,
         Media::Merger::LayerFactory
#if defined(HAS_VST2)
         ,
         Media::VST::LayerFactory
#endif
#if defined(LILV_SHARED)
         ,
         Media::LV2::LayerFactory
#endif
#if defined(HAS_FAUST)
         ,
         Media::Faust::LayerFactory
#endif
         >,
      FW<Library::LibraryInterface
#if defined(HAS_VST2)
         ,
         Media::VST::LibraryHandler
#endif
#if defined(LILV_SHARED)
         ,
         Media::LV2::LibraryHandler
#endif
         >,

#if defined(HAS_VST2)
      FW<Process::PortFactory, Media::VST::VSTControlPortFactory>,
#endif

      FW<Execution::ProcessComponentFactory, Execution::SoundComponentFactory,
         Media::EffectProcessComponentFactory, Execution::StepComponentFactory,
         Execution::MergerComponentFactory
#if defined(HAS_VST2)
         ,
         Execution::VSTEffectComponentFactory
#endif
#if defined(LILV_SHARED)
         ,
         Media::LV2::LV2EffectComponentFactory
#endif
#if defined(HAS_FAUST)
         ,
         Execution::FaustEffectComponentFactory
#endif
         >,
      FW<Scenario::DropHandler, Media::Sound::DropHandler>,
      FW<Scenario::IntervalDropHandler, Media::Sound::IntervalDropHandler>,
      FW<score::SettingsDelegateFactory, Media::Settings::Factory>>(ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_media)
