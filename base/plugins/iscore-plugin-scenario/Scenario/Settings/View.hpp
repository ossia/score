#pragma once
#include <iscore/plugins/settingsdelegate/SettingsDelegateViewInterface.hpp>

class QComboBox;
class QSpinBox;
namespace Scenario
{
namespace Settings
{

class View :
        public iscore::SettingsDelegateView
{
        Q_OBJECT
    public:
        View();

        void setSkin(const QString&);
        void setZoom(const int); // zoom percentage
        void setSlotHeight(const qreal);

    signals:
        void skinChanged(const QString&);
        void zoomChanged(int);
        void slotHeightChanged(qreal);

    private:
        QWidget* getWidget() override;
        QWidget* m_widg{};

        QComboBox* m_skin{};
        QSpinBox* m_zoomSpinBox{};
        QSpinBox* m_slotHeightBox{};
};

}
}
