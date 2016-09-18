#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Automation/AutomationModel.hpp>
#include <QString>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class QWidget;
class QCheckBox;

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

namespace Automation
{
class ProcessModel;
class InspectorWidget final :
        public Process::InspectorWidgetDelegate_T<Automation::ProcessModel>
{
    public:
        explicit InspectorWidget(
                const ProcessModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    private:
        void on_addressChange(const ::State::AddressAccessor& newText);
        void on_minValueChanged();
        void on_maxValueChanged();
        void on_tweenChanged();

        Explorer::AddressEditWidget* m_lineEdit{};
        QCheckBox* m_tween{};
        QDoubleSpinBox* m_minsb{}, *m_maxsb{};

        CommandDispatcher<> m_dispatcher;
};
}
