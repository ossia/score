#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <QString>

class JSProcessModel;
class QPlainTextEdit;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

class JSInspectorWidget final : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit JSInspectorWidget(
                const JSProcessModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    signals:
        void createViewInNewSlot(QString);

    private:
        void on_textChange(const QString& newText);

        const JSProcessModel& m_model;
        QPlainTextEdit* m_edit{};
};
