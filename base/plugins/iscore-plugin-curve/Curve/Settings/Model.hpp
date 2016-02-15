#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateModelInterface.hpp>
#include <iscore_plugin_curve_export.h>

namespace Curve
{
namespace Settings
{

struct Keys
{
        static const QString simplificationRatio;
};

class ISCORE_PLUGIN_CURVE_EXPORT Model :
        public iscore::SettingsDelegateModelInterface
{
        Q_OBJECT
        Q_PROPERTY(int simplificationRatio READ getSimplificationRatio WRITE setSimplificationRatio NOTIFY simplificationRatioChanged)

    public:
        Model();

        int getSimplificationRatio() const;
        void setSimplificationRatio(int getSimplificationRatio);

    signals:
        void simplificationRatioChanged(int getSimplificationRatio);

    private:
        void setFirstTimeSettings() override;
        int m_simplificationRatio = 50;
};

ISCORE_SETTINGS_PARAMETER(Model, SimplificationRatio)

}
}
