#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <Midi/MidiProcess.hpp>
class QComboBox;
namespace Midi
{
class InspectorWidget final :
        public Process::InspectorWidgetDelegate_T<Midi::ProcessModel>
{
    public:
        explicit InspectorWidget(
                const ProcessModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    private:
        void on_deviceChange(const QString& dev);

        QComboBox* m_devices;
};
class InspectorFactory final :
        public Process::InspectorWidgetDelegateFactory_T<ProcessModel, InspectorWidget>
{
        ISCORE_CONCRETE_FACTORY("78f380ff-a405-47b6-9d3b-7022af996199")
};
}
