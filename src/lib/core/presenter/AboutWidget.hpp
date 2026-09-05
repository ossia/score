#pragma once
#include <QColor>
#include <QFont>
#include <QWidget>

#include <score_lib_base_export.h>

namespace score
{
/**
 * @brief Version, partners and third-party licenses of ossia score.
 *
 * Shared by the start screen and the Help > About dialog so that both stay
 * identical. The look is driven by a Style so that the host can pass its own
 * fonts and colors; the defaults match the start screen.
 */
class SCORE_LIB_BASE_EXPORT AboutWidget final : public QWidget
{
public:
  struct Style
  {
    QFont sectionFont; //!< Tabs (uppercase)
    QFont itemFont;    //!< Regular text
    QFont smallFont;   //!< Captions, license text
    QColor text{"#f0f0f0"};
    QColor muted{"#8a8a8a"};
    QColor hover{"#03C3DD"};
    QColor version{"#0092CF"};
    QColor outline{"#3d3a3a"};
  };
  static Style defaultStyle();

  explicit AboutWidget(const Style& style, QWidget* parent = nullptr);
  ~AboutWidget();
};
}
