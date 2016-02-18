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
    on_modelChanged();

    this->setLayout(lay);
}

void InspectorWidget::on_modelChanged()
{
    const QList<RecordedMessage>& messages = process().messages();
    int n = messages.size();
    m_list->clear();
    m_list->setRowCount(n);
    auto dur = process().duration();

    for(int i = 0; i < n; i++)
    {
        auto& rm = messages[i];

        auto time = new QTableWidgetItem((dur * rm.percentage).toQTime().toString("hh:mm:ss.zzz"));
        auto addr = new QTableWidgetItem(rm.message.address.toString());
        auto val = new QTableWidgetItem(State::convert::toPrettyString(rm.message.value));

        m_list->setItem(i, 0, time);
        m_list->setItem(i, 1, addr);
        m_list->setItem(i, 2, val);
    }
}

}
