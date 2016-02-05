#include <State/Message.hpp>

#include "MessageEditDialog.hpp"
#include "MessageWidget.hpp"
#include <State/Address.hpp>
#include <State/Value.hpp>

class QWidget;

namespace Explorer
{
MessageWidget::MessageWidget(
        State::Message& mess,
        DeviceExplorerModel* model,
        QWidget* parent):
    QPushButton{mess.toString(), parent},
    m_model{model},
    m_message(mess)
{
    this->setFlat(true);
    connect(this, &QPushButton::clicked, this, &MessageWidget::on_clicked);
}

void MessageWidget::on_clicked()
{
    MessageEditDialog dial(m_message, m_model, this);
    int res = dial.exec();

    if(res)
    {
        // Update message
        m_message.address = dial.address();
        m_message.value = dial.value();

        this->setText(m_message.toString());
    }
}
}
