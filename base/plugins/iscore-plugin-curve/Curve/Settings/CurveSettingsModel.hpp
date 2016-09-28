#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <iscore_plugin_curve_export.h>

namespace Curve
{
namespace Settings
{

enum Mode
{
    Parameter, Message
};
}
}

Q_DECLARE_METATYPE(Curve::Settings::Mode)

namespace Curve
{
namespace Settings
{
class ISCORE_PLUGIN_CURVE_EXPORT Model :
        public iscore::SettingsDelegateModel
{
        Q_OBJECT
        Q_PROPERTY(int SimplificationRatio READ getSimplificationRatio WRITE setSimplificationRatio NOTIFY SimplificationRatioChanged FINAL)
        Q_PROPERTY(bool Simplify READ getSimplify WRITE setSimplify NOTIFY SimplifyChanged FINAL)
        Q_PROPERTY(Mode CurveMode READ getCurveMode WRITE setCurveMode NOTIFY CurveModeChanged FINAL)
        Q_PROPERTY(bool PlayWhileRecording READ getPlayWhileRecording WRITE setPlayWhileRecording NOTIFY PlayWhileRecordingChanged FINAL)

    public:
        Model(QSettings& set, const iscore::ApplicationContext& ctx);


        ISCORE_SETTINGS_PARAMETER_HPP(int, SimplificationRatio)
        ISCORE_SETTINGS_PARAMETER_HPP(bool, Simplify)
        ISCORE_SETTINGS_PARAMETER_HPP(Curve::Settings::Mode, CurveMode)
        ISCORE_SETTINGS_PARAMETER_HPP(bool, PlayWhileRecording)
    private:
        int m_SimplificationRatio{};
        bool m_Simplify = true;
        Mode m_CurveMode = Mode::Parameter;
        bool m_PlayWhileRecording{};
};

ISCORE_SETTINGS_PARAMETER(Model, SimplificationRatio)
ISCORE_SETTINGS_PARAMETER(Model, Simplify)
ISCORE_SETTINGS_PARAMETER(Model, CurveMode)
ISCORE_SETTINGS_PARAMETER(Model, PlayWhileRecording)

}
}
