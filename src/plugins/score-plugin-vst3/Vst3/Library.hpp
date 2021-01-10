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
  SCORE_CONCRETE("1d6ca523-628b-431a-9f70-87df92a63551")
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

    auto& plug = ctx.applicationPlugin<vst3::ApplicationPlugin>();

    auto reset_plugs = [=, &plug, &parent] {
      for (const auto& vst : plug.vst_infos)
      {
        if (vst.isValid)
        {
          auto name = QString::fromStdString(vst.module->getName());

          Library::ProcessData parent_data{
            {key, name, QString{}}, {}, {}, {}
          };
          auto& node = parent.emplace_back(std::move(parent_data), &parent);

          for(const auto& cls : vst.classInfo)
          {
            auto name = QString::fromStdString(cls.name());
            auto vendor = QString::fromStdString(cls.vendor());
            auto desc = QString::fromStdString(cls.version());
            auto uid = QString{"%1/::/%2"}.arg(vst.path).arg(name);
            qDebug() << " !!! " << name << uid;
            Library::ProcessData classdata{
              {key, name, uid}, {}, vendor, desc
            };
            if(parent.author.isEmpty())
              parent.author = vendor;
            if(vst.classInfo.size() == 1)
              parent.customData = uid;
            node.emplace_back(std::move(classdata), &node);
          }
        }
      }
    };

    reset_plugs();

    con(plug, &vst3::ApplicationPlugin::vstChanged, this, [=, &model, &parent] {
      model.beginResetModel();
      parent.resize(0);
      reset_plugs();
      model.endResetModel();
    });
  }
};
}
