#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>
#include <score/plugins/ProjectSettings/ProjectSettingsPresenter.hpp>

namespace Audio::Settings
{
class Model;
class View;
class Presenter : public score::GlobalSettingsPresenter
{
public:
  using model_type = Model;
  using view_type = View;
  Presenter(Model&, View&, QObject* parent);

private:
  QString settingsName() override;
  QIcon settingsIcon() override;
};
}
