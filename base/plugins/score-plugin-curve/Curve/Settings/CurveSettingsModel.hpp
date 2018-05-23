#pragma once
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <wobjectdefs.h>
#include <score_plugin_curve_export.h>

namespace Curve
{
namespace Settings
{

enum Mode
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

  SCORE_SETTINGS_PARAMETER_HPP(int, SimplificationRatio)
  SCORE_SETTINGS_PARAMETER_HPP(bool, Simplify)
  SCORE_SETTINGS_PARAMETER_HPP(Curve::Settings::Mode, CurveMode)
  SCORE_SETTINGS_PARAMETER_HPP(bool, PlayWhileRecording)

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
