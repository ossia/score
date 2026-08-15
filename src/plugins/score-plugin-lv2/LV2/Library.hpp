#pragma once
#include <LV2/ApplicationPlugin.hpp>
#include <LV2/EffectModel.hpp>
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/tools/Bind.hpp>

#include <QCoreApplication>
#include <QMap>
#include <QPointer>

namespace LV2
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("570f0b92-a091-47ff-a5c3-a585e07df2bf")

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    constexpr static const auto key = Metadata<ConcreteKey_k, LV2::Model>::get();

    auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();

    // Stage the whole database grouped by class; the model owns tree
    // mutation and signals.
    auto make_plugs = [&plug] {
      QMap<QString, Library::StagedNode> categories;
      for(const auto& info : plug.cachedDescriptors())
      {
        if(!info.valid)
          continue;
        QString category
            = info.class_label.isEmpty() ? QStringLiteral("Other") : info.class_label;
        auto it = categories.find(category);
        if(it == categories.end())
          it = categories.insert(
              category, Library::StagedNode{{{{}, category, {}}, {}}, {}});
        it->children.push_back(
            Library::StagedNode{{{key, info.name, info.uri}, {}}, {}});
      }

      std::vector<Library::StagedNode> v;
      v.reserve(categories.size());
      for(auto& cat : categories) // QMap: already name-sorted
        v.push_back(std::move(cat));
      return v;
    };

    model.clearAnchorKey(key);
    model.replaceChildren(key, make_plugs());

    QObject::connect(
        &plug, &LV2::ApplicationPlugin::descriptorsChanged, &model,
        [model = QPointer{&model}, make_plugs] {
      if(model)
        model->replaceChildren(key, make_plugs());
    });
  }
};
}
