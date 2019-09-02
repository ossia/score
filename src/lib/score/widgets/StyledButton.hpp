#pragma once
#include <QPushButton>

#include <score_lib_base_export.h>

#include <verdigris>

namespace score
{
class SCORE_LIB_BASE_EXPORT StyledButton : public QPushButton
{
  W_OBJECT(StyledButton)
public:
  StyledButton(QWidget* parent = 0);
};
}
