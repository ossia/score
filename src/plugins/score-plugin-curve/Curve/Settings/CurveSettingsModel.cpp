// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveSettingsModel.hpp"

#include <QSettings>

namespace Curve
{
namespace Settings
{

namespace Parameters
{
SETTINGS_PARAMETER_IMPL(SimplificationRatio){
    QStringLiteral("score_plugin_curve/SimplificationRatio"),
    10};
SETTINGS_PARAMETER_IMPL(Simplify){QStringLiteral("score_plugin_curve/Simplify"), true};
SETTINGS_PARAMETER_IMPL(CurveMode){QStringLiteral("score_plugin_curve/Mode"), Mode::Parameter};
SETTINGS_PARAMETER_IMPL(PlayWhileRecording){
    QStringLiteral("score_plugin_curve/PlayWhileRecording"),
    true};

static auto list()
{
  return std::tie(SimplificationRatio, Simplify, CurveMode, PlayWhileRecording);
}
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

SCORE_SETTINGS_PARAMETER_CPP(int, Model, SimplificationRatio)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, Simplify)
SCORE_SETTINGS_PARAMETER_CPP(Curve::Settings::Mode, Model, CurveMode)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, PlayWhileRecording)
}
}
