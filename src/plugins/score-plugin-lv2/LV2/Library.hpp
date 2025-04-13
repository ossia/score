#pragma once
#include <LV2/ApplicationPlugin.hpp>
#include <LV2/EffectModel.hpp>
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>

namespace LV2
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("570f0b92-a091-47ff-a5c3-a585e07df2bf")
  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    const auto& key = LV2::ProcessFactory{}.concreteKey();
    QModelIndex node = model.find(key);
    if(node == QModelIndex{})
    {
      return;
    }
    auto& parent = *reinterpret_cast<Library::ProcessNode*>(node.internalPointer());

    auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();
    auto& world = plug.lilv;

    auto plugs = world.get_all_plugins();

    ossia::flat_map<QString, QVector<QString>> categories;

    auto it = plugs.begin();
    while(!plugs.is_end(it))
    {
      auto plug = plugs.get(it);
      const auto class_name = plug.get_class().get_label().as_string();
      const auto plug_name = get_lv2_plugin_name(plug);
      categories[class_name].push_back(plug_name);
      it = plugs.next(it);
    }

    for(auto& category : categories)
    {
      // Already sorted through the map
      auto& cat = parent.emplace_back(
          Library::ProcessData{Process::ProcessData{{}, category.first, {}}, {}},
          &parent);
      for(auto& plug : category.second)
      {
        Library::addToLibrary(
            cat, Library::ProcessData{Process::ProcessData{key, plug, plug}, {}});
      }
    }
  }
};
}
