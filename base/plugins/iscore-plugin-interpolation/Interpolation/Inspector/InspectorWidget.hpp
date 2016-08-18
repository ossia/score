#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Explorer/Widgets/AddressEditWidget.hpp>
#include <Interpolation/Process.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>

class QCheckBox;

namespace Interpolation
{
class ProcessModel;
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
        void on_tweenChanged();

        Explorer::AddressEditWidget* m_lineEdit{};
        QCheckBox* m_tween{};

        CommandDispatcher<> m_dispatcher;
};
}
