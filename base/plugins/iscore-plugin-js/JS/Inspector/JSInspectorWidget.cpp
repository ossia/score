#include "JSInspectorWidget.hpp"
#include <JS/JSProcessModel.hpp>
#include <Inspector/InspectorSectionWidget.hpp>
#include "JS/Commands/EditScript.hpp"

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <iscore/widgets/SpinBoxes.hpp>

JSInspectorWidget::JSInspectorWidget(
        const JSProcessModel& JSModel,
        iscore::Document& doc,
        QWidget* parent) :
    InspectorWidgetBase {JSModel, doc, parent},
    m_model {JSModel}
{
    setObjectName("JSInspectorWidget");
    setParent(parent);

    m_edit = new QPlainTextEdit{JSModel.script()};
    connect(m_edit, &QPlainTextEdit::textChanged,
            this, [&] () {
        on_textChange(m_edit->toPlainText()); // TODO timer before validating ? TimerCommandDispatcher ?
    });

    updateSectionsView(safe_cast<QVBoxLayout*>(layout()), {m_edit});
}

void JSInspectorWidget::on_textChange(const QString& newTxt)
{
    auto cmd = new EditScript{m_model, newTxt};

    commandDispatcher()->submitCommand(cmd);
}
