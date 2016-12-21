#pragma once
#include <QBrush>
#include <QColor>
#include <eggs/variant.hpp>
#include <iscore/model/Skin.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <utility>

namespace iscore
{
/**
 * @brief A reference to a color. Used for skinning.
 *
 * This allows easy skinning : by using these classes instead of directly QColor,
 * we can change a color in a whole graphics scene instantly on the next redraw.
 */
struct ISCORE_LIB_BASE_EXPORT ColorRef
{
  friend bool operator==(ColorRef lhs, ColorRef rhs)
  {
    return lhs.ref == rhs.ref;
  }

  friend bool operator!=(ColorRef lhs, ColorRef rhs)
  {
    return lhs.ref != rhs.ref;
  }

public:
  ColorRef() = default;
  ColorRef(const ColorRef& other) = default;
  ColorRef(ColorRef&& other) = default;
  ColorRef& operator=(const ColorRef& other) = default;
  ColorRef& operator=(ColorRef&& other) = default;

  ColorRef(QColor Skin::*s) : ref{&(iscore::Skin::instance().*s)}
  {
  }

  ColorRef(const QColor* col) : ref{col}
  {
  }

  void setColor(QColor Skin::*s)
  {
    // Set color by reference
    ref = &(iscore::Skin::instance().*s);
  }

  QColor getColor() const
  {
    return ref ? *ref : Qt::black;
  }

  QBrush getBrush() const
  {
    return getColor();
  }

  QString name() const
  {
    return iscore::Skin::instance().toString(ref);
  }

  static optional<ColorRef> ColorFromString(const QString&);
  static optional<ColorRef> SimilarColor(QColor other);

private:
  const QColor* ref{};
};
}

Q_DECLARE_METATYPE(iscore::ColorRef)
