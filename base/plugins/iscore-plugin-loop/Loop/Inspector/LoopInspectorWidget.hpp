#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <QPlainTextEdit>
namespace iscore{
struct Address;
}
namespace Loop
{
class ProcessModel;
}
class DynamicProcessList;

class LoopInspectorWidget final : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit LoopInspectorWidget(
                const Loop::ProcessModel& object,
                iscore::Document& doc,
                QWidget* parent);

    signals:
        void createViewInNewSlot(QString);

    private:
        const Loop::ProcessModel& m_model;
};
