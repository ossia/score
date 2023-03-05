#pragma once
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Vst/ApplicationPlugin.hpp>
#include <Vst/EffectModel.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/detail/algorithms.hpp>

#include <algorithm>
namespace vst
{

class LibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("6a13c3cc-bca7-44d6-a0ef-644e99204460")
  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    constexpr static const auto key = Metadata<ConcreteKey_k, Model>::get();

    QModelIndex node = model.find(key);
    if(node == QModelIndex{})
    {
      return;
    }
    auto& parent = *reinterpret_cast<Library::ProcessNode*>(node.internalPointer());
    parent.key = {};

    auto& plug = ctx.applicationPlugin<vst::ApplicationPlugin>();

    auto reset_plugs = [=, &plug, &parent] {
      auto& fx = parent.emplace_back(
          Library::ProcessData{{{}, "Effects", {}}, {}, {}, {}}, &parent);
      auto& inst = parent.emplace_back(
          Library::ProcessData{{{}, "Instruments", {}}, {}, {}, {}}, &parent);
      for(const auto& vst : plug.vst_infos)
      {
        if(vst.isValid)
        {
          const auto& name
              = vst.displayName.isEmpty() ? vst.prettyName : vst.displayName;
          Library::ProcessData pdata{
              {key, name, QString::number(vst.uniqueID)}, {}, vst.author, {}};
          if(vst.isSynth)
          {
            Library::addToLibrary(inst, std::move(pdata));
          }
          else
          {
            Library::addToLibrary(fx, std::move(pdata));
          }
        }
      }
    };

    reset_plugs();

    con(plug, &vst::ApplicationPlugin::vstChanged, this,
        [&model, node, &parent, reset_plugs] {
      model.beginRemoveRows(node, 0, 1);
      parent.resize(0);
      model.endRemoveRows();

      model.beginInsertRows(node, 0, 1);
      reset_plugs();
      model.endInsertRows();
    });
  }
};
}
