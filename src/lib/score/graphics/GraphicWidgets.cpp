#include <score/graphics/BangPainting.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/model/Skin.hpp>

#include <QBrush>
#include <QPalette>

namespace score
{
QBrush bangFill(const QPalette& fallback, bool lit)
{
  const auto& skin = score::Skin::instance();
  const auto& b = lit ? skin.Base4.main.brush : skin.Emphasis2.main.brush;

  // A no-GUI application context builds Skin::NoGUI, whose brushes are left
  // default-constructed, i.e. Qt::NoBrush.
  if(b.style() != Qt::NoBrush && b.color().isValid() && b.color().alpha() > 0)
    return b;

  // Lit is the bright one, as Base4 is against Emphasis2.
  return QBrush{fallback.color(lit ? QPalette::Highlight : QPalette::Mid)};
}

// One per process: a right-click box is parented to the scene, so nothing
// else owns it.
QPointer<QGraphicsProxyWidget>& currentRightClickWidget()
{
  static QPointer<QGraphicsProxyWidget> current;
  return current;
}
}
