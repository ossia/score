#pragma once
#include <score/plugins/ProjectSettings/ProjectSettingsPresenter.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>

#include <Audio/AudioInterface.hpp>
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
  void on_accept() override;
  QString settingsName() override;
  QIcon settingsIcon() override;

  void loadDriver(const UuidKey<AudioFactory>& k);
};
}
