#pragma once
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <Vst3/EffectModel.hpp>
#include <Vst3/ApplicationPlugin.hpp>

#include <score/tools/Bind.hpp>

namespace vst3
{
class LibraryHandler final : public QObject, public Library::LibraryInterface
{
  SCORE_CONCRETE("e76eb5e4-4448-41b4-b292-8b37885b9754")
  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx) override
  {
    MSVC_BUGGY_CONSTEXPR static const auto key = Metadata<ConcreteKey_k, Model>::get();

    QModelIndex node = model.find(key);
    if (node == QModelIndex{})
    {
      return;
    }
    auto& parent = *reinterpret_cast<Library::ProcessNode*>(node.internalPointer());
    parent.key = {};

    auto& fx = parent.emplace_back(Library::ProcessData{{{}, "Effects", {}}, {}, {}, {}}, &parent);
    auto& inst = parent.emplace_back(Library::ProcessData{{{}, "Instruments", {}}, {}, {}, {}}, &parent);
    auto& other = parent.emplace_back(Library::ProcessData{{{}, "Other", {}}, {}, {}, {}}, &parent);
    auto& plug = ctx.applicationPlugin<vst3::ApplicationPlugin>();
/*
    auto reset_plugs = [=, &plug, &inst, &fx] {
      for (const auto& vst : plug.vst_infos)
      {
        if (vst.isValid)
        {
          const auto& name = vst.displayName.isEmpty() ? vst.prettyName : vst.displayName;
          Library::ProcessData pdata{
              {key, name, QString::number(vst.uniqueID)}, {}, vst.author, {}};
          if (vst.isSynth)
          {
            inst.emplace_back(std::move(pdata), &inst);
          }
          else
          {
            fx.emplace_back(std::move(pdata), &fx);
          }
        }
      }
    };

    reset_plugs();

    con(plug, &Media::ApplicationPlugin::vstChanged, this, [=, &model, &fx, &inst] {
      model.beginResetModel();
      fx.resize(0);
      inst.resize(0);
      reset_plugs();
      model.endResetModel();
    });
    */
  }
};
}
