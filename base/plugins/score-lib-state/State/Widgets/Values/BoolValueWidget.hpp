#pragma once
#include "ValueWidget.hpp"

#include <State/Value.hpp>

class QComboBox;
class QWidget;

namespace State
{
class SCORE_LIB_STATE_EXPORT BoolValueWidget final : public ValueWidget
{
public:
  BoolValueWidget(bool value, QWidget* parent = nullptr);

  ossia::value value() const override;

private:
  QComboBox* m_value;
};
}
