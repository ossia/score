#pragma once
#include <QWidget>
#include <iscore_lib_process_export.h>
namespace Process { class ProcessModel; }
class ISCORE_LIB_PROCESS_EXPORT ProcessInspectorWidgetDelegate : public QWidget
{
    public:
        using QWidget::QWidget;
        virtual ~ProcessInspectorWidgetDelegate();
        virtual const Process::ProcessModel& process() const = 0;
};

template<typename Process_T>
class ProcessInspectorWidgetDelegate_T : public ProcessInspectorWidgetDelegate
{
    public:
        ProcessInspectorWidgetDelegate_T(const Process_T& process, QWidget* parent):
            ProcessInspectorWidgetDelegate{parent},
            m_process{process}
        {

        }

        ~ProcessInspectorWidgetDelegate_T() = default;

        const Process_T& process() const final override
        { return m_process; }

    private:
        const Process_T& m_process;
};
