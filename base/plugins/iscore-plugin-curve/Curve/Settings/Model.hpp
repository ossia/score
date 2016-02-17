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
        static const QString simplify;
};

class ISCORE_PLUGIN_CURVE_EXPORT Model :
        public iscore::SettingsDelegateModelInterface
{
        Q_OBJECT
        Q_PROPERTY(int simplificationRatio READ getSimplificationRatio WRITE setSimplificationRatio NOTIFY simplificationRatioChanged)
        Q_PROPERTY(bool simplify READ getSimplify WRITE setSimplify NOTIFY simplifyChanged)

    public:
        Model();

        int getSimplificationRatio() const;
        void setSimplificationRatio(int getSimplificationRatio);

        bool getSimplify() const;
        void setSimplify(bool simplify);

    signals:
        void simplificationRatioChanged(int getSimplificationRatio);
        void simplifyChanged(bool simplify);

    private:
        void setFirstTimeSettings() override;
        int m_simplificationRatio = 50;
        bool m_simplify = true;
};

ISCORE_SETTINGS_PARAMETER(Model, SimplificationRatio)
ISCORE_SETTINGS_PARAMETER(Model, Simplify)

}
}
