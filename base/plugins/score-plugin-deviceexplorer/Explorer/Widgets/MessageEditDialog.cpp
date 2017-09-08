// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
//#include <State/Message.hpp>
//#include <State/ValueConversion.hpp>
//#include <State/Widgets/Values/BoolValueWidget.hpp>
//#include <State/Widgets/Values/CharValueWidget.hpp>
//#include <State/Widgets/Values/NumericValueWidget.hpp>
//#include <State/Widgets/Values/StringValueWidget.hpp>
//#include <eggs/variant/variant.hpp>
//#include <QChar>
//#include <QComboBox>
//#include <QDebug>
//#include <QDialogButtonBox>
//#include <QFlags>
//#include <QFormLayout>
//#include <QLayoutItem>

//#include <QString>

//#include "AddressEditWidget.hpp"
//#include <score/widgets/WidgetWrapper.hpp>
//#include "MessageEditDialog.hpp"
//#include <State/Value.hpp>
//#include <State/Widgets/Values/ValueWidget.hpp>
//#include <score/widgets/SignalUtils.hpp>
// class QWidget;

// namespace Explorer
//{
// MessageEditDialog::MessageEditDialog(const State::Message &mess,
// DeviceExplorerModel *model, QWidget *parent):
//    QDialog{parent},
//    m_message(mess)
//{
//    m_lay = new QFormLayout;
//    this->setLayout(m_lay);

//    m_addr = new AddressEditWidget{model, this};
//    m_addr->setAddress(mess.address);
//    m_lay->addWidget(m_addr);

//    m_typeCombo = new QComboBox;
//    connect(m_typeCombo, SignalUtils::QComboBox_currentIndexChanged_int(),
//            this, &MessageEditDialog::on_typeChanged);

//    m_val = new WidgetWrapper<ValueWidget>{this};
//    m_lay->addItem(new QSpacerItem(10, 10));
//    m_lay->addRow(tr("Type"), m_typeCombo);
//    m_lay->addRow(tr("Value"), m_val);

//    initTypeCombo();

//    auto buttons = new QDialogButtonBox(QDialogButtonBox::StandardButton::Ok
//    |
//                                        QDialogButtonBox::StandardButton::Cancel);
//    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
//    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

//    m_lay->addWidget(buttons);
//}

// const State::Address &MessageEditDialog::address() const
//{
//    return m_addr->address();
//}

// ossia::value MessageEditDialog::value() const
//{
//    if(m_val && m_val->widget())
//        return m_val->widget()->value();
//    else
//        return {};
//}

// void MessageEditDialog::initTypeCombo()
//{
//    // TODO sync with ValueConversion
//    m_typeCombo->insertItems(0, State::convert::ValuePrettyTypesList());
//    m_typeCombo->setCurrentIndex(m_message.value.v.which());
//}

// void MessageEditDialog::on_typeChanged(int t)
//{
//    // TODO refactor these widgets with the various address settings widgets
//    switch(ossia::val_type(t))
//    {
//        case ossia::val_type::NoValue:
//            m_val->setWidget(nullptr);
//            break;
//        case ossia::val_type::Impulse:
//            m_val->setWidget(nullptr);
//            break;
//        case ossia::val_type::Int:
//            m_val->setWidget(new
//            NumericValueWidget<int>(State::convert::value<int>(m_message.value),
//            this));
//            break;
//        case ossia::val_type::Float:
//            m_val->setWidget(new
//            NumericValueWidget<float>(State::convert::value<float>(m_message.value),
//            this));
//            break;
//        case ossia::val_type::Bool:
//            m_val->setWidget(new
//            BoolValueWidget(State::convert::value<bool>(m_message.value),
//            this));
//            break;
//        case ossia::val_type::String:
//            m_val->setWidget(new
//            StringValueWidget(State::convert::value<QString>(m_message.value),
//            this));
//            break;
//        case ossia::val_type::Char:
//            // TODO here a bug might be introduced : everywhere the char are
//            utf8 while here it's latin1.
//            m_val->setWidget(new
//            CharValueWidget(State::convert::value<QChar>(m_message.value).toLatin1(),
//            this));
//            break;
//        case ossia::val_type::Vec2f:
//        case ossia::val_type::Vec3f:
//        case ossia::val_type::Vec4f:
//        case ossia::val_type::List:
//            SCORE_TODO; // TODO Lists
//            break;
//        default:
//            SCORE_ABORT;
//            throw;
//    }
//}
//}
