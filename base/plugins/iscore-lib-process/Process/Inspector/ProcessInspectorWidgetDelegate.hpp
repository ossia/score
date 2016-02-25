#pragma once
#include <QWidget>
#include <iscore_lib_process_export.h>
namespace Process
{
class ProcessModel;
class ISCORE_LIB_PROCESS_EXPORT InspectorWidgetDelegate : public QWidget
{
    public:
        using QWidget::QWidget;
        virtual ~InspectorWidgetDelegate();
        virtual const Process::ProcessModel& process() const = 0;
};

template<typename Process_T>
class InspectorWidgetDelegate_T : public InspectorWidgetDelegate
{
    public:
        InspectorWidgetDelegate_T(
                const Process_T& process,
                QWidget* parent):
            InspectorWidgetDelegate{parent},
            m_process{process}
        {

        }

        ~InspectorWidgetDelegate_T() = default;

        const Process_T& process() const final override
        { return m_process; }

    private:
        const Process_T& m_process;
};

class StateProcess;
class ISCORE_LIB_PROCESS_EXPORT StateProcessInspectorWidgetDelegate : public QWidget
{
    public:
        using QWidget::QWidget;
        virtual ~StateProcessInspectorWidgetDelegate();
        virtual const Process::StateProcess& process() const = 0;
};

template<typename Process_T>
class StateProcessInspectorWidgetDelegate_T : public StateProcessInspectorWidgetDelegate
{
    public:
        StateProcessInspectorWidgetDelegate_T(
                const Process_T& process,
                QWidget* parent):
            StateProcessInspectorWidgetDelegate{parent},
            m_process{process}
        {

        }

        ~StateProcessInspectorWidgetDelegate_T() = default;

        const Process_T& process() const final override
        { return m_process; }

    private:
        const Process_T& m_process;
};
}
