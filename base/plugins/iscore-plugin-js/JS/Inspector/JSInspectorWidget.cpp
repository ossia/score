#include <JS/JSProcessModel.hpp>
#include <QPlainTextEdit>
#include <algorithm>

#include "Inspector/InspectorWidgetBase.hpp"
#include "JS/Commands/EditScript.hpp"
#include "JSInspectorWidget.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/tools/ModelPath.hpp>

class QVBoxLayout;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

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
