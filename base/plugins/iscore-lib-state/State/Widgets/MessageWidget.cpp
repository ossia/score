#include "MessageWidget.hpp"
#include <State/Message.hpp>

#include <QDialog>
#include <QLabel>
#include <QFormLayout>
#include <QComboBox>
#include <iscore/widgets/MarginLess.hpp>
class MessageEditDialog : public QDialog
{
    public:
        MessageEditDialog(const iscore::Message& mess, QWidget* parent):
            QDialog{parent}
        {
            auto lay = new MarginLess<QFormLayout>;
            this->setLayout(lay);

            auto typeCombo = new QComboBox;
            typeCombo->insertItems(0, {"None", "Int", "Float", "Char", "String", "Bool", "Tuple"});
            lay->addRow(tr("Address"), new QLabel{mess.address.toString()});
            lay->addRow(tr("Type"), typeCombo);
        }
};

MessageWidget::MessageWidget(
        const iscore::Message& mess,
        const CommandDispatcher<>& disp,
        QWidget* parent):
    QPushButton{mess.toString(), parent},
    m_message{mess}
{
    this->setFlat(true);
    connect(this, &QPushButton::clicked, this, &MessageWidget::on_clicked);

}

void MessageWidget::on_clicked()
{
    MessageEditDialog dial(m_message, this);
    int res = dial.exec();

    if(res)
    {
        // Update message

    }
    else
    {
        // Do nothing

    }
}
