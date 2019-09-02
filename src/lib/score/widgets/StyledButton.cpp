#include "StyledButton.hpp"

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::StyledButton)
namespace score
{
StyledButton::StyledButton(QWidget* parent) : QPushButton{parent}
{
  setMinimumSize(QSize(45, 45));
  setIconSize(QSize(35, 33));
}
}
