#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <QString>
#include <QTableWidget>

namespace RecordedMessages
{
class ProcessModel;
}
class QWidget;
namespace iscore {
class Document;
struct DocumentContext;
}  // namespace iscore

namespace RecordedMessages
{
class InspectorWidget final :
        public Process::InspectorWidgetDelegate_T<RecordedMessages::ProcessModel>
{
    public:
        explicit InspectorWidget(
                const RecordedMessages::ProcessModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    private:
        void on_modelChanged();

        CommandDispatcher<> m_dispatcher;
        QTableWidget* m_list{};
};
}
