#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <QString>

class QWidget;

namespace iscore{
class Document;
struct Address;
}
class AddressEditWidget;
class DeviceExplorerModel;
class MappingModel;
class QDoubleSpinBox;

class MappingInspectorWidget : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit MappingInspectorWidget(
                const MappingModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    signals:
        void createViewInNewSlot(QString);

    public slots:
        void on_sourceAddressChange(const iscore::Address& newText);
        void on_sourceMinValueChanged();
        void on_sourceMaxValueChanged();

        void on_targetAddressChange(const iscore::Address& newText);
        void on_targetMinValueChanged();
        void on_targetMaxValueChanged();
    private:
        AddressEditWidget* m_sourceLineEdit{};
        QDoubleSpinBox* m_sourceMin{}, *m_sourceMax{};

        AddressEditWidget* m_targetLineEdit{};
        QDoubleSpinBox* m_targetMin{}, *m_targetMax{};

        const MappingModel& m_model;
};
