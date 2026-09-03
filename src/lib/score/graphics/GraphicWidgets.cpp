#include <score/graphics/BangPainting.hpp>
#include <score/model/Skin.hpp>

#include <QBrush>
#include <QPalette>

namespace score
{
QBrush bangFill(const QPalette& fallback, bool lit)
{
  const auto& skin = score::Skin::instance();
  const auto& b = lit ? skin.Base4.main.brush : skin.Emphasis2.main.brush;

  // The skin comes from a resource the application installs; before that, or
  // in a test, its brushes are transparent black.
  if(b.style() != Qt::NoBrush && b.color().isValid() && b.color().alpha() > 0)
    return b;

  return QBrush{fallback.color(lit ? QPalette::Mid : QPalette::Highlight)};
}
}
