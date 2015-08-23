#include "MessageWidget.hpp"
#include <State/Message.hpp>

#include <QDialog>
#include <QLabel>
#include <QFormLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <iscore/widgets/MarginLess.hpp>

#include "Values/NumericValueWidget.hpp"
class ValueWrapper : public QWidget
{
    public:
        ValueWrapper(QWidget* parent):
            QWidget{parent}
        {
            m_lay = new MarginLess<QGridLayout>;
            this->setLayout(m_lay);
        }

        void setWidget(ValueWidget* widg)
        {
            clearLayout(m_lay);
            m_widget = widg;

            if(widg)
                m_lay->addWidget(m_widget);
        }

        ValueWidget* valueWidget() const
        { return m_widget; }

    private:
        void clearLayout(QLayout* layout)
        {
            QLayoutItem *child{};
            while ((child = layout->takeAt(0)) != 0)
            {
                if(child->layout() != 0)
                    clearLayout( child->layout() );
                else if(child->widget() != 0)
                    delete child->widget();

                delete child;
            }
        }

        QGridLayout* m_lay{};
        ValueWidget* m_widget{};
};

class MessageEditDialog : public QDialog
{
    public:
        MessageEditDialog(const iscore::Message& mess, QWidget* parent):
            QDialog{parent},
            m_message{mess}
        {
            m_lay = new QFormLayout;
            this->setLayout(m_lay);

            m_typeCombo = new QComboBox;
            m_typeCombo->insertItems(0, {"None", "Int", "Float", "Char", "String", "Bool", "Tuple"});

            m_val = new ValueWrapper{this};
            m_lay->addRow(tr("Address"), new QLabel{mess.address.toString()});
            m_lay->addRow(tr("Type"), m_typeCombo);
            m_lay->addRow(tr("Value"), m_val);

            connect(m_typeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                    this, &MessageEditDialog::on_typeChanged);

            auto buttons = new QDialogButtonBox(QDialogButtonBox::StandardButton::Ok |
                                                QDialogButtonBox::StandardButton::Cancel);
            connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);


            m_lay->addWidget(buttons);
        }

        QVariant value()
        {
            if(m_val && m_val->valueWidget())
                return m_val->valueWidget()->value();
            else
                return {};
        }

    private:
        void on_typeChanged(int t)
        {
            switch(t)
            {
                case 0:
                    m_val->setWidget(nullptr);
                    break;
                case 1:
                    m_val->setWidget(new NumericValueWidget<int>(m_message.value.val.toInt(), this));
                    break;
                case 2:
                    m_val->setWidget(new NumericValueWidget<float>(m_message.value.val.toFloat(), this));
                    break;
                case 3:
                    m_val->setWidget(new NumericValueWidget<char>(m_message.value.val.toChar().toLatin1(), this));
                    break;
                case 4:
                    m_val->setWidget(new StringValueWidget(m_message.value.val.toString(), this));
                    break;
                case 5:
                    m_val->setWidget(new BoolValueWidget(m_message.value.val.toBool(), this));
                    break;
                case 6:
                    ISCORE_TODO;
                    break;
                default:
                    Q_ASSERT(false);
            }
        }

        const iscore::Message& m_message;
        QFormLayout* m_lay{};
        QComboBox* m_typeCombo{};
        ValueWrapper* m_val{};
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
        qDebug() << dial.value();

    }
    else
    {
        // Do nothing

    }
}
