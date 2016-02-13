#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateViewInterface.hpp>

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
    signals:
        void simplificationRatioChanged(double);

    private:
        QWidget* getWidget() override;
        QWidget* m_widg{};

        QDoubleSpinBox* m_sb{};

};

}
}
