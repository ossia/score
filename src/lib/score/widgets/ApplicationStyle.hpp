#pragma once
#include <QProxyStyle>

#include <score_lib_base_export.h>

namespace score
{
/**
 * @brief Corrections over the base style for skins that ask for small text.
 *
 * QLineEdit::sizeHint and QComboBoxPrivate::recomputeSizeHint floor their
 * content height at 14 px whatever the font is, and floor it differently, so
 * each needs its own correction. Everything else is the wrapped style's.
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
