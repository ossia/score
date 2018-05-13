#pragma once
#include <QBrush>
#include <QColor>
#include <eggs/variant.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Todo.hpp>
#include <score/tools/std/Optional.hpp>
#include <utility>

namespace score
{
/**
 * @brief A reference to a color. Used for skinning.
 *
 * This allows easy skinning : by using these classes instead of directly
 * QColor, we can change a color in a whole graphics scene instantly on the
 * next redraw.
 */
struct SCORE_LIB_BASE_EXPORT ColorRef
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

  ColorRef(QBrush Skin::*s) : ref{&(score::Skin::instance().*s)}
  {
  }

  ColorRef(const QBrush* col) : ref{col}
  {
  }

  void setColor(QBrush Skin::*s)
  {
    // Set color by reference
    ref = &(score::Skin::instance().*s);
  }

  const QBrush& getBrush() const
  {
    SCORE_ASSERT(ref);
    return *ref;
  }

  QString name() const
  {
    return score::Skin::instance().toString(ref);
  }

  static optional<ColorRef> ColorFromString(const QString&);
  static optional<ColorRef> SimilarColor(QColor other);

private:
  const QBrush* ref{};
};
}

Q_DECLARE_METATYPE(score::ColorRef)
W_REGISTER_ARGTYPE(score::ColorRef)
