#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <QString>

class QWidget;

namespace iscore{
class Document;
struct Address;
}
class AddressEditWidget;
class AutomationModel;
class DeviceExplorerModel;
class QDoubleSpinBox;

class AutomationInspectorWidget final : public InspectorWidgetBase
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
