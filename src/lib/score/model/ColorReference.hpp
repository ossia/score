#pragma once
#include <score/model/Skin.hpp>
#include <score/tools/Debug.hpp>
#include <score/tools/std/Optional.hpp>

#include <QColor>

#include <eggs/variant.hpp>

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
  friend bool operator==(ColorRef lhs, ColorRef rhs) { return lhs.ref == rhs.ref; }

  friend bool operator!=(ColorRef lhs, ColorRef rhs) { return lhs.ref != rhs.ref; }

public:
  constexpr ColorRef() noexcept = default;
  constexpr ColorRef(const ColorRef& other) noexcept = default;
  constexpr ColorRef(ColorRef&& other) noexcept = default;
  constexpr ColorRef& operator=(const ColorRef& other) noexcept = default;
  constexpr ColorRef& operator=(ColorRef&& other) noexcept = default;

  ColorRef(Brush Skin::*s) : ref{&(score::Skin::instance().*s)} { }

  constexpr ColorRef(const Brush* col) noexcept : ref{col} { }

  void setColor(Brush Skin::*s) noexcept
  {
    // Set color by reference
    ref = &(score::Skin::instance().*s);
  }

  const Brush& getBrush() const
  {
    SCORE_ASSERT(ref);
    return *ref;
  }

  QString name() const noexcept { return score::Skin::instance().toString(ref); }

  static optional<ColorRef> ColorFromString(const QString&) noexcept;
  static optional<ColorRef> SimilarColor(QColor other) noexcept;

private:
  const Brush* ref{};
};
}

Q_DECLARE_METATYPE(score::ColorRef)
W_REGISTER_ARGTYPE(score::ColorRef)
