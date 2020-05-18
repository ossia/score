#pragma once
#include <Curve/Settings/CurveSettingsModel.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateView.hpp>

#include <verdigris>
class QCheckBox;
class QDoubleSpinBox;
namespace score{ class FormWidget;}

namespace Curve
{
namespace Settings
{

class View : public score::GlobalSettingsView
{
  W_OBJECT(View)
public:
  View();

  void setSimplificationRatio(int);
  void setSimplify(bool);
  void setMode(Mode);
  void setPlayWhileRecording(bool);

public:
  void simplificationRatioChanged(double arg_1)
      E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, simplificationRatioChanged, arg_1);
  void simplifyChanged(bool arg_1) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, simplifyChanged, arg_1);
  void modeChanged(Mode arg_1) E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, modeChanged, arg_1);
  void playWhileRecordingChanged(bool arg_1)
      E_SIGNAL(SCORE_PLUGIN_CURVE_EXPORT, playWhileRecordingChanged, arg_1);

private:
  QWidget* getWidget() override;
  score::FormWidget* m_widg{};

  QDoubleSpinBox* m_sb{};
  QCheckBox* m_simpl{};
  QCheckBox* m_mode{};
  QCheckBox* m_playWhileRecording{};
};
}
}
