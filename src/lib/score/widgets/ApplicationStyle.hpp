#pragma once
#include <QProxyStyle>

#include <score_lib_base_export.h>

namespace score
{
/**
 * @brief Corrections over the base style for skins that ask for small text.
 *
 * Qt computes the height of a text-entry widget as at least 14 pixels of
 * content whatever the font is -- see QLineEdit::sizeHint and
 * QComboBoxPrivate::recomputeSizeHint, which floor differently and have to be
 * corrected separately. That floor was written for desktop type; under a skin
 * whose application font is 8 px it leaves a full-size box around a line of
 * text half its height, and every form in the inspector ends up mostly
 * padding.
 *
 * Everything else is the wrapped style's.
 */
class SCORE_LIB_BASE_EXPORT ApplicationStyle final : public QProxyStyle
{
public:
  using QProxyStyle::QProxyStyle;

  QSize sizeFromContents(
      ContentsType type, const QStyleOption* option, const QSize& size,
      const QWidget* widget) const override;
};
}
