#include <RecordedMessages/RecordedMessagesProcessModel.hpp>
#include <algorithm>

#include <Inspector/InspectorWidgetBase.hpp>
#include "RecordedMessages/Commands/EditMessages.hpp"
#include "RecordedMessagesInspectorWidget.hpp"
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <QVBoxLayout>

class QVBoxLayout;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore
namespace RecordedMessages
{
InspectorWidget::InspectorWidget(
        const RecordedMessages::ProcessModel& RecordedMessagesModel,
        const iscore::DocumentContext& doc,
        QWidget* parent) :
    ProcessInspectorWidgetDelegate_T {RecordedMessagesModel, parent},
    m_dispatcher{doc.commandStack},
    m_list{new QTableWidget}
{
    setObjectName("RecordedMessagesInspectorWidget");
    setParent(parent);
    auto lay = new QVBoxLayout;
    lay->addWidget(m_list);

    m_list->setColumnCount(3);
    m_list->setHorizontalHeaderLabels({tr("Time"), tr("Address"), tr("Value")});

    con(process(), &RecordedMessages::ProcessModel::messagesChanged,
        this, &InspectorWidget::on_modelChanged);

    this->setLayout(lay);
}

void InspectorWidget::on_modelChanged()
{
    const QList<RecordedMessage>& messages = process().messages();
    int n = messages.size();
    m_list->clear();
    m_list->setRowCount(n);
    for(int i = 0; i < n; i++)
    {
        auto& rm = messages[i];
        m_list->item(i, 0)->setText(QString::number(rm.time.msec()));
        m_list->item(i, 1)->setText(rm.message.address.toString());
        m_list->item(i, 2)->setText(State::convert::toPrettyString(rm.message.value));
    }


}

}
