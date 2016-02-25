#pragma once
#include <QVBoxLayout>
#include <Process/Inspector/ProcessInspectorWidgetDelegate.hpp>

class QWidget;
namespace iscore {
class Document;
struct DocumentContext;
}  // namespace iscore

namespace Loop
{
class ProcessModel;
}

class LoopInspectorWidget final :
        public Process::InspectorWidgetDelegate_T<Loop::ProcessModel>
{
    public:
        explicit LoopInspectorWidget(
                const Loop::ProcessModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);
};
