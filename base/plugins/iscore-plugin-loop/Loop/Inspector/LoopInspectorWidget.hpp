#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <QString>

class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Loop
{
class ProcessModel;
}

class LoopInspectorWidget final : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit LoopInspectorWidget(
                const Loop::ProcessModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    signals:
        void createViewInNewSlot(QString);

    private:
        const Loop::ProcessModel& m_model;
};
