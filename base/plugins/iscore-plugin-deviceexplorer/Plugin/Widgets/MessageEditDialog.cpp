#include "MessageEditDialog.hpp"

#include "Plugin/Panel/DeviceExplorerModel.hpp"
#include "AddressEditWidget.hpp"
#include "ValueWrapper.hpp"
#include <State/Widgets/Values/NumericValueWidget.hpp>
#include <State/Widgets/Values/StringValueWidget.hpp>
#include <State/Widgets/Values/BoolValueWidget.hpp>

#include <State/Message.hpp>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QComboBox>

MessageEditDialog::MessageEditDialog(const iscore::Message &mess, DeviceExplorerModel *model, QWidget *parent):
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

const iscore::Address &MessageEditDialog::address() const
{
    return m_addr->address();
}

QVariant MessageEditDialog::value() const
{
    if(m_val && m_val->valueWidget())
        return m_val->valueWidget()->value();
    else
        return {};
}

void MessageEditDialog::initTypeCombo()
{
    m_typeCombo->insertItems(0, {tr("None"), tr("Int"), tr("Float"), tr("Char"), tr("String"), tr("Bool"), tr("Tuple")});

    switch(static_cast<QMetaType::Type>(m_message.value.val.type()))
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

void MessageEditDialog::on_typeChanged(int t)
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
            ISCORE_ABORT;
    }
}
