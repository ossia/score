#include "ArrowButton.hpp"

#include <score/widgets/SetIcons.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::ArrowButton)
namespace score
{
std::array<QString, 5> arrow_name = {
    "",     // NoArrow
    "up",   // UpArrow
    "down", // DownArrow
    "left", // LeftArrow
    "right" // RightArrow
};
ArrowButton::ArrowButton(Qt::ArrowType arrowType, QWidget* parent)
  : QToolButton{parent}
  , m_arrowType{arrowType}
{
  setArrowType(arrowType);
  setIconSize(QSize(8, 8));
  setAutoRaise(true);
}

void ArrowButton::setArrowType(Qt::ArrowType type)
{
  if (m_arrowType == type || type == Qt::NoArrow)
    return;

  m_arrowType = type;

  setIcon(makeIcons(
      ":/icons/arrow_" + arrow_name[m_arrowType] + "_on.png",
      ":/icons/arrow_" + arrow_name[m_arrowType] + "_off.png",
      ":/icons/arrow_" + arrow_name[m_arrowType] + "_disabled.png"));
}

}
