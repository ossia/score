#include <JS/JSProcessModel.hpp>
#include "NotifyingPlainTextEdit.hpp"
#include <algorithm>

#include <Inspector/InspectorWidgetBase.hpp>
#include "JS/Commands/EditScript.hpp"
#include "JSInspectorWidget.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <QVBoxLayout>

class QVBoxLayout;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

JSInspectorWidget::JSInspectorWidget(
        const JS::ProcessModel& JSModel,
        const iscore::DocumentContext& doc,
        QWidget* parent) :
    ProcessInspectorWidgetDelegate_T {JSModel, parent},
    m_dispatcher{doc.commandStack}
{
    setObjectName("JSInspectorWidget");
    setParent(parent);
    auto lay = new QVBoxLayout;

    m_edit = new NotifyingPlainTextEdit{JSModel.script()};
    connect(m_edit, &NotifyingPlainTextEdit::editingFinished,
            this, &JSInspectorWidget::on_textChange);

    con(process(), &JS::ProcessModel::scriptChanged,
            this, &JSInspectorWidget::on_modelChanged);

    m_script = m_edit->toPlainText();

    lay->addWidget(m_edit);
    this->setLayout(lay);
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

    auto cmd = new JS::EditScript{process(), newTxt};

    m_dispatcher.submitCommand(cmd);
}
