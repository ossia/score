#pragma once
#include <QWidget>
#include <iscore_lib_process_export.h>
#include <Process/Process.hpp>

namespace iscore
{
struct DocumentContext;
}
class ProcessInspectorWidgetDelegate;

class ISCORE_LIB_PROCESS_EXPORT ProcessInspectorWidget final : public QWidget
{
        Q_OBJECT

    public:
        ProcessInspectorWidget(
                ProcessInspectorWidgetDelegate* delegate,
                const iscore::DocumentContext& doc,
                QWidget* parent);

    signals:
        void createViewInNewSlot(const Id<Process::ProcessModel>&);

    private:
        ProcessInspectorWidgetDelegate* m_delegate{};
        const iscore::DocumentContext& m_context;
};
