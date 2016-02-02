#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
class QWidget;

namespace iscore{
class Document;
struct DocumentContext;
}
namespace State
{
struct Address;
}

namespace Explorer
{
class AddressEditWidget;
class DeviceExplorerModel;
}
class QDoubleSpinBox;

namespace Mapping
{
class ProcessModel;
class MappingInspectorWidget :
        public ProcessInspectorWidgetDelegate_T<ProcessModel>
{
    public:
        explicit MappingInspectorWidget(
                const ProcessModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    public slots:
        void on_sourceAddressChange(const State::Address& newText);
        void on_sourceMinValueChanged();
        void on_sourceMaxValueChanged();

        void on_targetAddressChange(const State::Address& newText);
        void on_targetMinValueChanged();
        void on_targetMaxValueChanged();
    private:
        Explorer::AddressEditWidget* m_sourceLineEdit{};
        QDoubleSpinBox* m_sourceMin{}, *m_sourceMax{};

        Explorer::AddressEditWidget* m_targetLineEdit{};
        QDoubleSpinBox* m_targetMin{}, *m_targetMax{};

        CommandDispatcher<> m_dispatcher;
};
}
