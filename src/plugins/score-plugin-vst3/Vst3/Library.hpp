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

  void registerVSTClass(
      Library::ProcessNode& parent, const AvailablePlugin& vst,
      const VST3::Hosting::ClassInfo& cls)
  {
    constexpr static const auto key = Metadata<ConcreteKey_k, Model>::get();

    auto name = QString::fromStdString(cls.name());
    auto uid = QString::fromStdString(cls.ID().toString());

    Library::ProcessData classdata{{key, name, uid}, {}};
    if(vst.classInfo.size() == 1)
      parent.customData = uid;
    Library::addToLibrary(parent, std::move(classdata));
  }

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

    auto& plug = ctx.applicationPlugin<vst3::ApplicationPlugin>();

    auto reset_plugs = [this, &plug, &parent] {
      for(const auto& vst : plug.vst_infos)
      {
        if(vst.isValid)
        {
          Library::ProcessData parent_data{{key, vst.name, QString{}}, {}};

          const int numClasses = vst.classInfo.size();
          switch(numClasses)
          {
            default: {
              auto& node = Library::addToLibrary(parent, std::move(parent_data));

              for(const auto& cls : vst.classInfo)
              {
                registerVSTClass(node, vst, cls);
              }
              break;
            }
            case 1: {
              registerVSTClass(parent, vst, vst.classInfo[0]);
              break;
            }
            case 0:
              break;
          }
        }
      }
    };

    reset_plugs();

    con(plug, &vst3::ApplicationPlugin::vstChanged, this,
        [&plug, &model, node, &parent, reset_plugs] {
      if(parent.childCount() > 0)
      {
        model.beginRemoveRows(node, 0, parent.childCount() - 1);
        parent.resize(0);
        model.endRemoveRows();
      }

      int k = 0;
      for(const auto& vst : plug.vst_infos)
        if(vst.isValid)
          k++;
      if(k > 0)
      {
        model.beginInsertRows(node, 0, k - 1);
        reset_plugs();
        model.endInsertRows();
      }
    });
  }
};
}
