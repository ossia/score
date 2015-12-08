#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <QString>

class JSProcessModel;
class NotifyingPlainTextEdit;
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

    public slots:
        void on_modelChanged(const QString& script);

    private:
        void on_textChange(const QString& newText);

        const JSProcessModel& m_model;
        NotifyingPlainTextEdit* m_edit{};
        QString m_script;
};
