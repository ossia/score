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

    auto& plug = ctx.applicationPlugin<vst::ApplicationPlugin>();

    // Stage the whole plugin list; the model owns tree mutation and signals.
    auto make_plugs = [&plug] {
      Library::StagedNode fx{{{{}, "Effects", {}}, {}}, {}};
      Library::StagedNode inst{{{{}, "Instruments", {}}, {}}, {}};
      for(const auto& vst : plug.vst_infos)
      {
        if(vst.isValid)
        {
          const auto& name
              = vst.displayName.isEmpty() ? vst.prettyName : vst.displayName;
          Library::StagedNode p{{{key, name, QString::number(vst.uniqueID)}, {}}, {}};
          (vst.isSynth ? inst : fx).children.push_back(std::move(p));
        }
      }
      std::vector<Library::StagedNode> v;
      v.push_back(std::move(fx));
      v.push_back(std::move(inst));
      return v;
    };

    model.clearAnchorKey(key);
    model.replaceChildren(key, make_plugs());

    con(plug, &vst::ApplicationPlugin::vstChanged, this,
        [model = QPointer{&model}, make_plugs] {
      if(model)
        model->replaceChildren(key, make_plugs());
    });
  }
};
}
