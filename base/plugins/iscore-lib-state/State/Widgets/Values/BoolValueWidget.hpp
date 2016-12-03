#pragma once
#include "ValueWidget.hpp"
#include <State/Value.hpp>

class QComboBox;
class QWidget;

namespace State
{
class ISCORE_LIB_STATE_EXPORT BoolValueWidget final : public State::ValueWidget
{
public:
  BoolValueWidget(bool value, QWidget* parent = nullptr);

  State::Value value() const override;

private:
  QComboBox* m_value;
};
}
