/*
#pragma once
#if defined(HAS_VST2)
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Media/ApplicationPlugin.hpp>
#include <Media/Effect/VST/VSTEffectModel.hpp>
#include <score/tools/Bind.hpp>

namespace vst3
{
class LibraryHandler final : public QObject, public Library::LibraryInterface
{
  SCORE_CONCRETE("6a13c3cc-bca7-44d6-a0ef-644e99204460")
  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx) override
  {
    MSVC_BUGGY_CONSTEXPR static const auto key = Metadata<ConcreteKey_k, VSTEffectModel>::get();

    QModelIndex node = model.find(key);
    if (node == QModelIndex{})
    {
      return;
    }
    auto& parent = *reinterpret_cast<Library::ProcessNode*>(node.internalPointer());
    parent.key = {};

    auto& fx = parent.emplace_back(Library::ProcessData{{{}, "Effects", {}}, {}, {}, {}}, &parent);
    auto& inst = parent.emplace_back(Library::ProcessData{{{}, "Instruments", {}}, {}, {}, {}}, &parent);
    auto& plug = ctx.applicationPlugin<Media::ApplicationPlugin>();

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
  }
};
}
#endif
*/
