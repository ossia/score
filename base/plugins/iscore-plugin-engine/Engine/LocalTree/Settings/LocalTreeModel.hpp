#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>

namespace Engine
{
namespace LocalTree
{
namespace Settings
{

class Model : public iscore::SettingsDelegateModel
{
  Q_OBJECT
  Q_PROPERTY(bool LocalTree READ getLocalTree WRITE setLocalTree NOTIFY
                 LocalTreeChanged)

  bool m_LocalTree = false;

public:
  Model(QSettings& set, const iscore::ApplicationContext& ctx);

  ISCORE_SETTINGS_PARAMETER_HPP(bool, LocalTree)
};

ISCORE_SETTINGS_DEFERRED_PARAMETER(Model, LocalTree)
}
}
}
