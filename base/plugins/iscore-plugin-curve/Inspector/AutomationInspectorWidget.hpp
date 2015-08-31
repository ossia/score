#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
namespace iscore{
struct Address;
}
class DeviceExplorerModel;
class AutomationModel;
class QDoubleSpinBox;
class AddressEditWidget;
class AutomationInspectorWidget : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit AutomationInspectorWidget(
                const AutomationModel& object,
                iscore::Document& doc,
                QWidget* parent);

    signals:
        void createViewInNewSlot(QString);

    public slots:
        void on_addressChange(const iscore::Address& newText);
        void on_minValueChanged();
        void on_maxValueChanged();

    private:
        DeviceExplorerModel* m_explorer{};
        AddressEditWidget* m_lineEdit{};
        QDoubleSpinBox* m_minsb{}, *m_maxsb{};
        const AutomationModel& m_model;
};
