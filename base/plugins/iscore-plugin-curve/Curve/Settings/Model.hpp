#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp>
#include <iscore_plugin_curve_export.h>

namespace Curve
{
namespace Settings
{

enum class Mode
{
    Parameter, Message
};

struct Keys
{
        static const QString simplificationRatio;
        static const QString simplify;
        static const QString mode;
};

class ISCORE_PLUGIN_CURVE_EXPORT Model :
        public iscore::SettingsDelegateModelInterface
{
        Q_OBJECT
        Q_PROPERTY(int simplificationRatio READ getSimplificationRatio WRITE setSimplificationRatio NOTIFY SimplificationRatioChanged)
        Q_PROPERTY(bool simplify READ getSimplify WRITE setSimplify NOTIFY SimplifyChanged)
        Q_PROPERTY(Mode mode READ getMode WRITE setMode NOTIFY ModeChanged)

    public:
        Model();

        int getSimplificationRatio() const;
        void setSimplificationRatio(int getSimplificationRatio);

        bool getSimplify() const;
        void setSimplify(bool simplify);

        Mode getMode() const;
        void setMode(Mode getMode);

    signals:
        void SimplificationRatioChanged(int getSimplificationRatio);
        void SimplifyChanged(bool simplify);
        void ModeChanged(Mode getMode);

    private:
        void setFirstTimeSettings() override;
        int m_simplificationRatio = 50;
        bool m_simplify = true;
        Mode m_mode = Mode::Parameter;
};

ISCORE_SETTINGS_PARAMETER(Model, SimplificationRatio)
ISCORE_SETTINGS_PARAMETER(Model, Simplify)
ISCORE_SETTINGS_PARAMETER(Model, Mode)

}
}
