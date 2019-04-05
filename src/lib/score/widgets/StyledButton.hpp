#pragma once
#include <QPushButton>
#include <wobjectdefs.h>

namespace score
{
  class StyledButton : public QPushButton
  {
      W_OBJECT(StyledButton)
  public:
    StyledButton(QWidget* parent = 0);
  };
}
