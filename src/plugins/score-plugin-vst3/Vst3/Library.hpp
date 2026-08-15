#pragma once
#include <Library/LibraryInterface.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Vst3/ApplicationPlugin.hpp>
#include <Vst3/EffectModel.hpp>

#include <score/tools/Bind.hpp>

namespace vst3
{
class LibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("1d6ca523-628b-431a-9f70-87df92a63551")

  static Library::StagedNode stagedVSTClass(const VST3::Hosting::ClassInfo& cls)
  {
    constexpr static const auto key = Metadata<ConcreteKey_k, Model>::get();

    auto name = QString::fromStdString(cls.name());
    auto uid = QString::fromStdString(cls.ID().toString());
    return Library::StagedNode{{{key, name, uid}, {}}, {}};
  }

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    constexpr static const auto key = Metadata<ConcreteKey_k, Model>::get();

    auto& plug = ctx.applicationPlugin<vst3::ApplicationPlugin>();

    // Stage the whole plugin list; the model owns tree mutation and signals.
    auto make_plugs = [&plug] {
      std::vector<Library::StagedNode> v;
      for(const auto& vst : plug.vst_infos)
      {
        if(vst.isValid)
        {
          const int numClasses = vst.classInfo.size();
          switch(numClasses)
          {
            default: {
              Library::StagedNode p{{{key, vst.name, QString{}}, {}}, {}};
              for(const auto& cls : vst.classInfo)
                p.children.push_back(stagedVSTClass(cls));
              v.push_back(std::move(p));
              break;
            }
            case 1: {
              v.push_back(stagedVSTClass(vst.classInfo[0]));
              break;
            }
            case 0:
              break;
          }
        }
      }
      std::sort(v.begin(), v.end(), [](const auto& lhs, const auto& rhs) {
        return QString::compare(
                   lhs.data.prettyName, rhs.data.prettyName, Qt::CaseInsensitive)
               < 0;
      });
      return v;
    };

    model.clearAnchorKey(key);
    model.replaceChildren(key, make_plugs());

    con(plug, &vst3::ApplicationPlugin::vstChanged, this,
        [model = QPointer{&model}, make_plugs] {
      if(model)
        model->replaceChildren(key, make_plugs());
    });
  }
};
}
