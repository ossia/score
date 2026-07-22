#pragma once
#include <LV2/ApplicationPlugin.hpp>
#include <LV2/EffectModel.hpp>
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/tools/Bind.hpp>

#include <QCoreApplication>
#include <QMap>

namespace LV2
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("570f0b92-a091-47ff-a5c3-a585e07df2bf")

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    constexpr static const auto key = Metadata<ConcreteKey_k, LV2::Model>::get();

    QModelIndex node = model.find(key);
    if(node == QModelIndex{})
      return;

    auto& parent = *reinterpret_cast<Library::ProcessNode*>(node.internalPointer());
    parent.key = {};

    auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();

    auto reset_plugs = [&plug, &parent] {
      QMap<QString, Library::ProcessNode*> categories;
      for(const auto& info : plug.cachedDescriptors())
      {
        if(!info.valid)
          continue;
        QString category
            = info.class_label.isEmpty() ? QStringLiteral("Other") : info.class_label;
        if(!categories.contains(category))
        {
          auto& cat_node = parent.emplace_back(
              Library::ProcessData{{{}, category, {}}, {}}, &parent);
          categories[category] = &cat_node;
        }
        Library::ProcessData pdata{{key, info.name, info.uri}, {}};
        Library::addToLibrary(*categories[category], std::move(pdata));
      }
    };

    reset_plugs();

    // Full reset: tree edits below are plain-data; partial signals would dangle indexes
    QObject::connect(
        &plug, &LV2::ApplicationPlugin::descriptorsChanged, &model,
        [&model, &parent, reset_plugs] {
      model.beginResetModel();
      parent.resize(0);
      reset_plugs();
      model.endResetModel();
    });
  }
};
}
