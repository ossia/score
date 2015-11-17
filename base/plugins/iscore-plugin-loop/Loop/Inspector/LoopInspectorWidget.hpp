#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <QPlainTextEdit>
namespace iscore{
struct Address;
}
class LoopProcessModel;

class LoopInspectorWidget final : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit LoopInspectorWidget(
                const LoopProcessModel& object,
                iscore::Document& doc,
                QWidget* parent);

    signals:
        void createViewInNewSlot(QString);

    private:
        const LoopProcessModel& m_model;
};
