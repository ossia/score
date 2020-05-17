#pragma once
#if defined(HAS_LV2)
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Media/ApplicationPlugin.hpp>
#include <Media/Effect/LV2/LV2EffectModel.hpp>

namespace Media::LV2
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("570f0b92-a091-47ff-a5c3-a585e07df2bf")
  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx) override
  {
    const auto& key = LV2EffectFactory{}.concreteKey();
    QModelIndex node = model.find(key);
    if (node == QModelIndex{})
    {
      return;
    }
    auto& parent = *reinterpret_cast<Library::ProcessNode*>(node.internalPointer());

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
      auto& cat = parent.emplace_back(Library::ProcessData{{{}, category.first, {}}, {}}, &parent);
      for (auto& plug : category.second)
      {
        Library::ProcessData pdata{{key, plug, plug}, {}};
        cat.emplace_back(pdata, &cat);
      }
    }
  }
};
}
#endif
