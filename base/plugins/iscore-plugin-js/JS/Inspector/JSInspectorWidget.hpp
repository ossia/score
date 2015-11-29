#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <qstring.h>

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
                iscore::Document& doc,
                QWidget* parent);

    signals:
        void createViewInNewSlot(QString);

    private:
        void on_textChange(const QString& newText);

        const JSProcessModel& m_model;
        QPlainTextEdit* m_edit{};
};
