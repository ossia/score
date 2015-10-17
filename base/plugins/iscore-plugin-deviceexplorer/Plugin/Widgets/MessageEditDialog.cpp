#include "MessageEditDialog.hpp"

#include "Plugin/Panel/DeviceExplorerModel.hpp"
#include "AddressEditWidget.hpp"
#include <State/Widgets/Values/NumericValueWidget.hpp>
#include <State/Widgets/Values/StringValueWidget.hpp>
#include <State/Widgets/Values/BoolValueWidget.hpp>
#include <State/ValueConversion.hpp>
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

    m_val = new WidgetWrapper<ValueWidget>{this};
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

iscore::Value MessageEditDialog::value() const
{
    if(m_val && m_val->widget())
        return m_val->widget()->value();
    else
        return {};
}

void MessageEditDialog::initTypeCombo()
{
    // TODO sync with ValueConversion
    m_typeCombo->insertItems(0, iscore::convert::prettyTypes());
    m_typeCombo->setCurrentIndex(m_message.value.val.impl().which());
}

#include <State/ValueConversion.hpp>
void MessageEditDialog::on_typeChanged(int t)
{
    // TODO use an enum
    // TODO refactor these widgets with the various address settings widgets
    switch(t)
    {
        case 0:
            m_val->setWidget(nullptr);
            break;
        case 1:
            m_val->setWidget(new NumericValueWidget<int>(iscore::convert::value<int>(m_message.value), this));
            break;
        case 2:
            m_val->setWidget(new NumericValueWidget<float>(iscore::convert::value<float>(m_message.value), this));
            break;
        case 3:
            m_val->setWidget(new BoolValueWidget(iscore::convert::value<bool>(m_message.value), this));
            break;
        case 4:
            m_val->setWidget(new StringValueWidget(iscore::convert::value<QString>(m_message.value), this));
            break;
        case 5:
            // TODO here a bug might be introduced : everywhere the char are utf8 while here it's latin1.
            m_val->setWidget(new CharValueWidget(iscore::convert::value<QChar>(m_message.value).toLatin1(), this));
            break;
        case 6:
            ISCORE_TODO; // TODO Tuples
            break;
        default:
            ISCORE_ABORT;
    }
}
