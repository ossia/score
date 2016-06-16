#include "ValueWidget.hpp"
#include <State/ValueConversion.hpp>
#include <iscore/widgets/MarginLess.hpp>
#include <State/Widgets/Values/BoolValueWidget.hpp>
#include <State/Widgets/Values/CharValueWidget.hpp>
#include <State/Widgets/Values/NumericValueWidget.hpp>
#include <State/Widgets/Values/StringValueWidget.hpp>
#include <State/Widgets/Values/TypeComboBox.hpp>

#include <QHBoxLayout>
namespace State
{
ValueWidget::~ValueWidget() = default;

TypeAndValueWidget::~TypeAndValueWidget() = default;


TypeAndValueWidget::TypeAndValueWidget(
        State::Value init,
        QWidget* parent):
    QWidget{parent},
    m_type{new TypeComboBox{this}},
    m_val{new WidgetWrapper<State::ValueWidget>{this}}
{
    auto lay = new iscore::MarginLess<QHBoxLayout>{this};

    lay->addWidget(m_type);
    auto t = m_type->currentType();
    ISCORE_TODO;
}

Value TypeAndValueWidget::value() const
{
    if(m_val)
        return m_val->widget()->value();

    return State::Value::fromValue(impulse_t{});
}

// TODO refactor with MessageEditDialog
void TypeAndValueWidget::on_typeChanged(State::Value val, State::ValueType t)
{
    // TODO refactor these widgets with the various address settings widgets
    switch(t)
    {
        case State::ValueType::NoValue:
            ISCORE_ABORT;
            break;
        case State::ValueType::Impulse:
            m_val->setWidget(nullptr);
            break;
        case State::ValueType::Int:
            m_val->setWidget(new NumericValueWidget<int>(State::convert::value<int>(val), this));
            break;
        case State::ValueType::Float:
            m_val->setWidget(new NumericValueWidget<float>(State::convert::value<float>(val), this));
            break;
        case State::ValueType::Bool:
            m_val->setWidget(new BoolValueWidget(State::convert::value<bool>(val), this));
            break;
        case State::ValueType::String:
            m_val->setWidget(new StringValueWidget(State::convert::value<QString>(val), this));
            break;
        case State::ValueType::Char:
            // TODO here a bug might be introduced : everywhere the char are utf8 while here it's latin1.
            m_val->setWidget(new CharValueWidget(State::convert::value<QChar>(val).toLatin1(), this));
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
