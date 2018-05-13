#pragma once
#include <Curve/Settings/CurveSettingsModel.hpp>
#include <wobjectdefs.h>
#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>
class QCheckBox;
class QDoubleSpinBox;
namespace Curve
{
namespace Settings
{

class View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

  void setSimplificationRatio(double);
  void setSimplify(bool);
  void setMode(Mode);
  void setPlayWhileRecording(bool);

public:
  void simplificationRatioChanged(double arg_1) W_SIGNAL(simplificationRatioChanged, arg_1);
  void simplifyChanged(bool arg_1) W_SIGNAL(simplifyChanged, arg_1);
  void modeChanged(Mode arg_1) W_SIGNAL(modeChanged, arg_1);
  void playWhileRecordingChanged(bool arg_1) W_SIGNAL(playWhileRecordingChanged, arg_1);

private:
  QWidget* getWidget() override;
  QWidget* m_widg{};

  QDoubleSpinBox* m_sb{};
  QCheckBox* m_simpl{};
  QCheckBox* m_mode{};
  QCheckBox* m_playWhileRecording{};
};
}
}
