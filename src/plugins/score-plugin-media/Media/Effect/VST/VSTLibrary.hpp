#pragma once
#if defined(HAS_VST2)
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Media/ApplicationPlugin.hpp>
#include <Media/Effect/VST/VSTEffectModel.hpp>

namespace Media::VST
{
class LibraryHandler final : public QObject, public Library::LibraryInterface
{
  SCORE_CONCRETE("6a13c3cc-bca7-44d6-a0ef-644e99204460")
  void setup(
      Library::ProcessesItemModel& model,
      const score::GUIApplicationContext& ctx) override
  {
    MSVC_BUGGY_CONSTEXPR static const auto key
        = Metadata<ConcreteKey_k, VSTEffectModel>::get();

    QModelIndex node = model.find(key);
    if (node == QModelIndex{})
    {
      return;
    }
    auto& parent
        = *reinterpret_cast<Library::ProcessNode*>(node.internalPointer());

    auto& fx = parent.emplace_back(
        Library::ProcessData{"Effects", QIcon{}, {}, {}}, &parent);
    auto& inst = parent.emplace_back(
        Library::ProcessData{"Instruments", QIcon{}, {}, {}}, &parent);
    auto& plug = ctx.applicationPlugin<Media::ApplicationPlugin>();

    auto reset_plugs = [=, &plug, &inst, &fx] {
      for (const auto& vst : plug.vst_infos)
      {
        if (vst.isValid)
        {
          QJsonObject obj;
          obj["Type"] = "Process";
          obj["uuid"] = toJsonValue(key.impl());
          obj["Data"] = QString::number(vst.uniqueID);

          const auto& name
              = vst.displayName.isEmpty() ? vst.prettyName : vst.displayName;
          if (vst.isSynth)
          {
            inst.emplace_back(
                Library::ProcessData{name, QIcon{}, obj, key}, &inst);
          }
          else
          {
            fx.emplace_back(
                Library::ProcessData{name, QIcon{}, obj, key}, &fx);
          }
        }
      }
    };

    reset_plugs();

    con(plug,
        &Media::ApplicationPlugin::vstChanged,
        this,
        [=, &model, &fx, &inst] {
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
