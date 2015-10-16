#include "MessageWidget.hpp"

#include "MessageEditDialog.hpp"

#include <State/Message.hpp>
#include <DeviceExplorer/../Plugin/Widgets/AddressEditWidget.hpp>
#include <iscore/widgets/MarginLess.hpp>

#include <QDialog>
#include <QLabel>
#include <QFormLayout>
#include <QComboBox>
#include <QDialogButtonBox>

MessageWidget::MessageWidget(
        iscore::Message& mess,
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
        m_message.value = iscore::Value::fromQVariant(dial.value());

        this->setText(m_message.toString());
    }
}
