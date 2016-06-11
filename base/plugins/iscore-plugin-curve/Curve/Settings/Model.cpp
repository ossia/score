#include "Model.hpp"
#include <QSettings>

namespace Curve
{
namespace Settings
{

namespace Parameters
{
        const iscore::sp<ModelSimplificationRatioParameter> SimplificationRatio{
            QStringLiteral("iscore_plugin_curve/SimplificationRatio"),
                    10};
        const iscore::sp<ModelSimplifyParameter> Simplify{
            QStringLiteral("iscore_plugin_curve/Simplify"),
                    true};
        const iscore::sp<ModelCurveModeParameter> CurveMode{
            QStringLiteral("iscore_plugin_curve/Mode"),
                    Mode::Parameter};
        const iscore::sp<ModelPlayWhileRecordingParameter> PlayWhileRecording{
            QStringLiteral("iscore_plugin_curve/PlayWhileRecording"),
                    true};

        auto list() {
            return std::tie(SimplificationRatio, Simplify, CurveMode, PlayWhileRecording);
        }
}

Model::Model(QSettings& set, const iscore::ApplicationContext& ctx)
{
    iscore::setupDefaultSettings(set, Parameters::list(), *this);
}

ISCORE_SETTINGS_PARAMETER_CPP(int, Model, SimplificationRatio)
ISCORE_SETTINGS_PARAMETER_CPP(bool, Model, Simplify)
ISCORE_SETTINGS_PARAMETER_CPP(Curve::Settings::Mode, Model, CurveMode)
ISCORE_SETTINGS_PARAMETER_CPP(bool, Model, PlayWhileRecording)
}
}
