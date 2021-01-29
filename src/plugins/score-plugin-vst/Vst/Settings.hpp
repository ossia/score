#pragma once
#include <Vst/ApplicationPlugin.hpp>

#include <Media/Effect/Settings/Model.hpp>
#include <Media/Effect/Settings/View.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegatePresenter.hpp>
#include <verdigris>

class QListWidget;

namespace Vst
{

class SettingsWidget
    : public Media::Settings::PluginSettingsTab
{
  W_OBJECT(SettingsWidget)
  SCORE_CONCRETE("849b6420-cdc9-47c3-9cac-74897336a77a")
public:
  using View = SettingsWidget;
  using Model = Media::Settings::Model;

  explicit SettingsWidget();

  void setVstPaths(QStringList val);

  QString name() const noexcept override;
  QWidget* make(const score::ApplicationContext& ctx) override;

public:
  void VstPathsChanged(QStringList arg_1) W_SIGNAL(VstPathsChanged, arg_1);

private:
  Model* m_model{};
  QListWidget* m_VstPaths{};
  QStringList m_curitems;

  score::SettingsCommandDispatcher m_disp;
  Model& model(SettingsWidget* self);
};


}
