#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <score_plugin_curve_export.h>

#include <verdigris>

namespace Curve
{
namespace Settings
{

enum Mode : bool
{
  Parameter,
  Message
};
}
}

Q_DECLARE_METATYPE(Curve::Settings::Mode)
W_REGISTER_ARGTYPE(Curve::Settings::Mode)

namespace Curve
{
namespace Settings
{
class SCORE_PLUGIN_CURVE_EXPORT Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_CURVE_EXPORT, int, SimplificationRatio)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_CURVE_EXPORT, bool, Simplify)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_CURVE_EXPORT, Curve::Settings::Mode, CurveMode)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_CURVE_EXPORT, bool, PlayWhileRecording)

private:
  int m_SimplificationRatio{};
  bool m_Simplify = true;
  Mode m_CurveMode = Mode::Parameter;
  bool m_PlayWhileRecording{};
};

SCORE_SETTINGS_PARAMETER(Model, SimplificationRatio)
SCORE_SETTINGS_PARAMETER(Model, Simplify)
SCORE_SETTINGS_PARAMETER(Model, CurveMode)
SCORE_SETTINGS_PARAMETER(Model, PlayWhileRecording)
}
}
