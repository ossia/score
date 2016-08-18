#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Explorer/Widgets/AddressEditWidget.hpp>
#include <Interpolation/Process.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

namespace Interpolation
{
class ProcessModel;
/**
 * Note : this class is temporarily in iscore-plugin-scenario instead
 * of iscore-plugin-interpolation, because of the need to access the first and
 * last state upon address change, which can be done only in the context of a scenario.
 */
class InspectorWidget final :
        public Process::InspectorWidgetDelegate_T<Interpolation::ProcessModel>
{
    public:
        explicit InspectorWidget(
                const ProcessModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    private:
        void on_addressChange(const ::State::Address& newText);
        Explorer::AddressEditWidget* m_lineEdit{};

        CommandDispatcher<> m_dispatcher;
};

class InspectorFactory final :
        public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
        ISCORE_CONCRETE_FACTORY("5159eabc-cd5c-4a00-a790-bd58936aace0")
};
}
