#include "ApplicationStyle.hpp"

#include <QFontMetrics>
#include <QStyleOption>
#include <QWidget>

namespace score
{

QSize ApplicationStyle::sizeFromContents(
    ContentsType type, const QStyleOption* option, const QSize& size,
    const QWidget* widget) const
{
  auto sz = QProxyStyle::sizeFromContents(type, option, size, widget);

  switch(type)
  {
    case CT_LineEdit:
    case CT_ComboBox: {
      // The widget floors its own content height before the style ever sees
      // it, so what arrives here is that floor rather than the height of the
      // text. Hand back the difference; once the text is the taller of the
      // two this changes nothing, which is every skin at a usual font size.
      const int floor = qMax(14, pixelMetric(PM_SmallIconSize, option, widget) - 2);
      const int text
          = option ? option->fontMetrics.height()
                   : (widget ? widget->fontMetrics().height() : floor);
      if(text < floor)
        sz.rheight() -= floor - text;
      break;
    }
    default:
      break;
  }

  return sz;
}

}
