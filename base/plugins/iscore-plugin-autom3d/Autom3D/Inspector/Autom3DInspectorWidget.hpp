#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Autom3D/Autom3DModel.hpp>
#include <QString>
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
class AddressEditWidget;
class DeviceExplorerModel;
class QDoubleSpinBox;

namespace Autom3D
{
class ProcessModel;
class InspectorWidget final :
        public ProcessInspectorWidgetDelegate_T<Autom3D::ProcessModel>
{
    public:
        explicit InspectorWidget(
                const ProcessModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    public slots:
        void on_addressChange(const ::State::Address& newText);

    private:
        AddressEditWidget* m_lineEdit{};

        CommandDispatcher<> m_dispatcher;
};
}
