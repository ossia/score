#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

namespace Engine
{
namespace LocalTree
{
namespace Settings
{

class Model : public score::SettingsDelegateModel
{
  Q_OBJECT
  Q_PROPERTY(bool LocalTree READ getLocalTree WRITE setLocalTree NOTIFY
                 LocalTreeChanged)

  bool m_LocalTree = false;

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  SCORE_SETTINGS_PARAMETER_HPP(bool, LocalTree)
};

SCORE_SETTINGS_DEFERRED_PARAMETER(Model, LocalTree)
}
}
}
