#include "ApplicationStyle.hpp"

#include <QComboBox>
#include <QFontMetrics>
#include <QStyleOption>
#include <QWidget>
#include <QtMath>

namespace score
{
namespace
{
//! Mirrors QComboBoxPrivate::recomputeSizeHint: the policy counts as a
//! promise of an icon even before an item carries one.
bool comboHasIcon(const QComboBox& cb) noexcept
{
  if(cb.sizeAdjustPolicy() == QComboBox::AdjustToMinimumContentsLengthWithIcon)
    return true;

  const int n = cb.count();
  for(int i = 0; i < n; i++)
    if(!cb.itemIcon(i).isNull())
      return true;
  return false;
}
}

QSize ApplicationStyle::sizeFromContents(
    ContentsType type, const QStyleOption* option, const QSize& size,
    const QWidget* widget) const
{
  auto sz = QProxyStyle::sizeFromContents(type, option, size, widget);
  if(!option)
    return sz;

  switch(type)
  {
    case CT_LineEdit: {
      // QLineEdit::sizeHint applies qMax(fm.height(), qMax(14, smallIcon - 2))
      // before the style sees it. Hand back the difference.
      const int floor = qMax(14, pixelMetric(PM_SmallIconSize, option, widget) - 2);
      const int text = option->fontMetrics.height();
      if(text < floor)
        sz.rheight() -= floor - text;
      break;
    }

    case CT_ComboBox: {
      // recomputeSizeHint floors against a literal 14, not PM_SmallIconSize,
      // then raises it again for the item icon. That second floor is the
      // icon's room, not slack.
      const int text = qCeil(QFontMetricsF{option->fontMetrics}.height());
      if(text >= 14)
        break;

      int had = qMax(text, 14) + 2;
      int want = text + 2;
      if(const auto* cb = qobject_cast<const QComboBox*>(widget); cb && comboHasIcon(*cb))
      {
        const int icon = cb->iconSize().height() + 2;
        had = qMax(had, icon);
        want = qMax(want, icon);
      }
      sz.rheight() -= had - want;
      break;
    }

    default:
      break;
  }

  return sz;
}

}
