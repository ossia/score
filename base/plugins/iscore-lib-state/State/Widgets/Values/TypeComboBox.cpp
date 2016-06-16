#include "TypeComboBox.hpp"
#include <State/ValueConversion.hpp>
#include <iscore/widgets/SignalUtils.hpp>

namespace State
{

ValueType TypeComboBox::currentType() const
{
    return this->currentData().value<State::ValueType>();
}

TypeComboBox::TypeComboBox(QWidget* parent):
    QComboBox{parent}
{
    for(auto& tp : State::convert::ValuePrettyTypesMap())
        this->addItem(tp.first, QVariant::fromValue(tp.second));

    connect(this, SignalUtils::QComboBox_currentIndexChanged_int(),
            this, [=] (int i) {
        emit typeChanged(this->itemData(i).value<State::ValueType>());
    });
}

TypeComboBox::~TypeComboBox()
{

}
}
