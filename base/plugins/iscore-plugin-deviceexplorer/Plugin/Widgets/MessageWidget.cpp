#include "MessageWidget.hpp"
#include <State/Message.hpp>

#include <QDialog>
#include <QLabel>
#include <QFormLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <iscore/widgets/MarginLess.hpp>

#include <State/Widgets/Values/NumericValueWidget.hpp>
#include <DeviceExplorer/../Plugin/Widgets/AddressEditWidget.hpp>

static void clearLayout(QLayout* layout)
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
        QGridLayout* m_lay{};
        ValueWidget* m_widget{};
};

class MessageEditDialog : public QDialog
{
    public:
        MessageEditDialog(const iscore::Message& mess, DeviceExplorerModel* model, QWidget* parent):
            QDialog{parent},
            m_message(mess)
        {
            m_lay = new QFormLayout;
            this->setLayout(m_lay);

            m_addr = new AddressEditWidget{model, this};
            m_addr->setAddress(mess.address);
            m_lay->addWidget(m_addr);

            m_typeCombo = new QComboBox;
            connect(m_typeCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                    this, &MessageEditDialog::on_typeChanged);

            m_val = new ValueWrapper{this};
            m_lay->addItem(new QSpacerItem(10, 10));
            m_lay->addRow(tr("Type"), m_typeCombo);
            m_lay->addRow(tr("Value"), m_val);


            initTypeCombo();

            auto buttons = new QDialogButtonBox(QDialogButtonBox::StandardButton::Ok |
                                                QDialogButtonBox::StandardButton::Cancel);
            connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
            connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

            m_lay->addWidget(buttons);
        }


        const iscore::Address& address() const
        {
            return m_addr->address();
        }

        QVariant value() const
        {
            if(m_val && m_val->valueWidget())
                return m_val->valueWidget()->value();
            else
                return {};
        }

    private:
        void initTypeCombo()
        {
            m_typeCombo->insertItems(0, {tr("None"), tr("Int"), tr("Float"), tr("Char"), tr("String"), tr("Bool"), tr("Tuple")});

            switch(QMetaType::Type(m_message.value.val.type()))
            {
                case QMetaType::Int:
                    m_typeCombo->setCurrentIndex(1);
                    break;
                case QMetaType::Float:
                    m_typeCombo->setCurrentIndex(2);
                    break;
                case QMetaType::Char:
                    m_typeCombo->setCurrentIndex(3);
                    break;
                case QMetaType::QString:
                    m_typeCombo->setCurrentIndex(4);
                    break;
                case QMetaType::Bool:
                    m_typeCombo->setCurrentIndex(5);
                    break;
                case QMetaType::QVariantList:
                    m_typeCombo->setCurrentIndex(6);
                    break;
                default:
                    m_typeCombo->setCurrentIndex(0);
                    break;
            }
        }

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

        AddressEditWidget* m_addr{};

        QFormLayout* m_lay{};
        QComboBox* m_typeCombo{};
        ValueWrapper* m_val{};
};

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
        m_message.value.val = dial.value();

        this->setText(m_message.toString());
    }
}

MessageListEditor::MessageListEditor(
        const iscore::MessageList& m,
        DeviceExplorerModel* model,
        QWidget* parent):
    m_model{model},
    m_messages{m}
{
    auto lay = new QGridLayout;

    m_messageListLayout = new QGridLayout;
    lay->addLayout(m_messageListLayout, 0, 0);
    updateLayout();

    auto addButton = new QPushButton(tr("Add message"));
    lay->addWidget(addButton);
    connect(addButton, &QPushButton::clicked,
            this, &MessageListEditor::addMessage);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::StandardButton::Ok |
                                        QDialogButtonBox::StandardButton::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    lay->addWidget(buttons);

    this->setLayout(lay);
}

void MessageListEditor::addMessage()
{
    MessageEditDialog dial(iscore::Message(), m_model, this);
    int res = dial.exec();

    if(res)
    {
        m_messages.push_back({dial.address(), dial.value()});
    }

    updateLayout();
}

void MessageListEditor::removeMessage(int i)
{
    m_messages.removeAt(i);
    updateLayout();
}

void MessageListEditor::updateLayout()
{
    clearLayout(m_messageListLayout);
    int i = 0;
    for(auto& mess : m_messages)
    {
        m_messageListLayout->addWidget(new MessageWidget{mess, m_model, this}, i, 0);

        auto removeButton = new QPushButton(tr("Remove"));
        m_messageListLayout->addWidget(removeButton, i, 1);

        connect(removeButton,&QPushButton::pressed,
                this, [=] { removeMessage(i); });
        i++;
    }
}
