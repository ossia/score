#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateViewInterface.hpp>
#include <QCheckBox>
class QDoubleSpinBox;
namespace Curve
{
namespace Settings
{

class View :
        public iscore::SettingsDelegateViewInterface
{
        Q_OBJECT
    public:
        View();

        void setSimplificationRatio(double);
        void setSimplify(bool);

    signals:
        void simplificationRatioChanged(double);
        void simplifyChanged(bool);

    private:
        QWidget* getWidget() override;
        QWidget* m_widg{};

        QDoubleSpinBox* m_sb{};
        QCheckBox* m_simpl{};

};

}
}
