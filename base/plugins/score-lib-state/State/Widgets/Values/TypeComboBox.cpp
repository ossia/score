// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "TypeComboBox.hpp"
#include <State/ValueConversion.hpp>
#include <score/widgets/SignalUtils.hpp>

namespace State
{

ossia::val_type TypeComboBox::get() const
{
  return this->currentData().value<ossia::val_type>();
}

void TypeComboBox::set(ossia::val_type t)
{
  if(t != ossia::val_type::NONE)
    setCurrentIndex((int) t);
  else
    setCurrentIndex(this->count() - 1);
}

TypeComboBox::TypeComboBox(QWidget* parent) : QComboBox{parent}
{
  auto& arr = State::convert::ValuePrettyTypesArray();
  const int n = arr.size();
  for (int i = 0; i < n - 1; i++)
  {
    auto t = static_cast<ossia::val_type>(i);
    addItem(arr[i], QVariant::fromValue(t));
  }
  addItem(arr[n - 1], QVariant::fromValue(ossia::val_type::NONE));

  connect(
      this, SignalUtils::QComboBox_currentIndexChanged_int(), this,
      [=](int i) {
        changed(this->itemData(i).value<ossia::val_type>());
      });
}

TypeComboBox::~TypeComboBox()
{
}
}
