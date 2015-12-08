#include <JS/JSProcessModel.hpp>
#include "NotifyingPlainTextEdit.hpp"
#include <algorithm>

#include <Inspector/InspectorWidgetBase.hpp>
#include "JS/Commands/EditScript.hpp"
#include "JSInspectorWidget.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QVBoxLayout>

class QVBoxLayout;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

JSInspectorWidget::JSInspectorWidget(
        const JSProcessModel& JSModel,
        const iscore::DocumentContext& doc,
        QWidget* parent) :
    InspectorWidgetBase {JSModel, doc, parent},
    m_model {JSModel}
{
    setObjectName("JSInspectorWidget");
    setParent(parent);

    m_edit = new NotifyingPlainTextEdit{JSModel.script()};
    connect(m_edit, &NotifyingPlainTextEdit::editingFinished,
            this, &JSInspectorWidget::on_textChange);

    con(m_model, &JSProcessModel::scriptChanged,
            this, &JSInspectorWidget::on_modelChanged);

    updateSectionsView(safe_cast<QVBoxLayout*>(layout()), {m_edit});
    m_script = m_edit->toPlainText();
}

void JSInspectorWidget::on_modelChanged(const QString& script)
{
    m_script = script;
    m_edit->setPlainText(script);
}

void JSInspectorWidget::on_textChange(const QString& newTxt)
{
    if(newTxt == m_script)
        return;

    auto cmd = new EditScript{m_model, newTxt};

    commandDispatcher()->submitCommand(cmd);
}
