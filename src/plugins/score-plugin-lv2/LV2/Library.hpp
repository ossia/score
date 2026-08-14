#pragma once
#include <LV2/ApplicationPlugin.hpp>
#include <LV2/EffectModel.hpp>
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/tools/ThreadPool.hpp>

#include <QCoreApplication>

namespace LV2
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("570f0b92-a091-47ff-a5c3-a585e07df2bf")
  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    score::TaskPool::instance().post([model = QPointer{&model}, &ctx] {
      auto& plug = ctx.applicationPlugin<LV2::ApplicationPlugin>();
      auto& world = plug.lilv;

      // This part can take a few seconds so we do it in threadpool
      auto plugs = world.get_all_plugins();

      ossia::flat_map<QString, QVector<QString>> categories;
      categories.reserve(50);

      {
        auto lck = std::unique_lock{plug.library_lock};
        if(plug.abort_library_scan)
          return;

        auto it = plugs.begin();
        while(!plugs.is_end(it))
        {
          if(plug.abort_library_scan)
            return;

          auto plug = plugs.get(it);
          const auto class_name = plug.get_class().get_label().as_string();
          const auto plug_name = get_lv2_plugin_name(plug);
          categories[class_name].push_back(plug_name);
          it = plugs.next(it);
        }
      }

      // Back to main thread
      QMetaObject::invokeMethod(
          QCoreApplication::instance(), [model, categories = std::move(categories)] {
        if(!model)
          return;

        const auto& key = LV2::ProcessFactory{}.concreteKey();
        QModelIndex node = model->find(key);
        if(node == QModelIndex{})
          return;

        auto& parent = *reinterpret_cast<Library::ProcessNode*>(node.internalPointer());

        // Build the subtree off to the side, then append it under a single
        // begin/endInsertRows instead of mutating the live model silently.
        Library::ProcessNode built;
        for(auto& category : categories)
        {
          // Already sorted through the map
          auto& cat = built.emplace_back(
              Library::ProcessData{Process::ProcessData{{}, category.first, {}}, {}},
              &built);
          for(auto& plug : category.second)
          {
            Library::addToLibrary(
                cat, Library::ProcessData{Process::ProcessData{key, plug, plug}, {}});
          }
        }

        if(const int n = built.childCount(); n > 0)
        {
          const int base = parent.childCount();
          model->beginInsertRows(node, base, base + n - 1);
          built.moveChildren(parent);
          model->endInsertRows();
        }
      });
    });
  }
};
}
