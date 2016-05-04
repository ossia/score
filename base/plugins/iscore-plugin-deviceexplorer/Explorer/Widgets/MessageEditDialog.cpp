#include <State/Message.hpp>
#include <State/ValueConversion.hpp>
#include <State/Widgets/Values/BoolValueWidget.hpp>
#include <State/Widgets/Values/CharValueWidget.hpp>
#include <State/Widgets/Values/NumericValueWidget.hpp>
#include <State/Widgets/Values/StringValueWidget.hpp>
#include <eggs/variant/variant.hpp>
#include <QChar>
#include <QComboBox>
#include <QDebug>
#include <QDialogButtonBox>
#include <QFlags>
#include <QFormLayout>
#include <QLayoutItem>

#include <QString>

#include "AddressEditWidget.hpp"
#include <Explorer/Widgets/ValueWrapper.hpp>
#include "MessageEditDialog.hpp"
#include <State/Value.hpp>
#include <State/Widgets/Values/ValueWidget.hpp>
#include <iscore/widgets/SignalUtils.hpp>
class QWidget;

namespace Explorer
{
MessageEditDialog::MessageEditDialog(const State::Message &mess, DeviceExplorerModel *model, QWidget *parent):
    QDialog{parent},
    m_message(mess)
{
    m_lay = new QFormLayout;
    this->setLayout(m_lay);

    m_addr = new AddressEditWidget{model, this};
    m_addr->setAddress(mess.address);
    m_lay->addWidget(m_addr);

    m_typeCombo = new QComboBox;
    connect(m_typeCombo, SignalUtils::QComboBox_currentIndexChanged_int(),
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

const State::Address &MessageEditDialog::address() const
{
    return m_addr->address();
}

State::Value MessageEditDialog::value() const
{
    if(m_val && m_val->widget())
        return m_val->widget()->value();
    else
        return {};
}

void MessageEditDialog::initTypeCombo()
{
    // TODO sync with ValueConversion
    m_typeCombo->insertItems(0, State::convert::ValuePrettyTypesList());
    m_typeCombo->setCurrentIndex(m_message.value.val.impl().which());
}

void MessageEditDialog::on_typeChanged(int t)
{
    // TODO refactor these widgets with the various address settings widgets
    switch(State::ValueType(t))
    {
        case State::ValueType::NoValue:
            m_val->setWidget(nullptr);
            break;
        case State::ValueType::Impulse:
            m_val->setWidget(nullptr);
            break;
        case State::ValueType::Int:
            m_val->setWidget(new NumericValueWidget<int>(State::convert::value<int>(m_message.value), this));
            break;
        case State::ValueType::Float:
            m_val->setWidget(new NumericValueWidget<float>(State::convert::value<float>(m_message.value), this));
            break;
        case State::ValueType::Bool:
            m_val->setWidget(new BoolValueWidget(State::convert::value<bool>(m_message.value), this));
            break;
        case State::ValueType::String:
            m_val->setWidget(new StringValueWidget(State::convert::value<QString>(m_message.value), this));
            break;
        case State::ValueType::Char:
            // TODO here a bug might be introduced : everywhere the char are utf8 while here it's latin1.
            m_val->setWidget(new CharValueWidget(State::convert::value<QChar>(m_message.value).toLatin1(), this));
            break;
        case State::ValueType::Tuple:
            ISCORE_TODO; // TODO Tuples
            break;
        default:
            ISCORE_ABORT;
            throw;
    }
}
}
