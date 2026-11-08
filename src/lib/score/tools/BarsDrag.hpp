#pragma once
#include <QPointF>

#include <algorithm>
#include <cmath>
#include <vector>

namespace score
{
/**
 * @brief A modifier drag over a row of bars, each the value of a step.
 *
 * Offset (Shift) moves every bar by the same amount, as far as the mouse went
 * up or down. Random (Alt) blends each bar towards a random target, the more
 * the further the mouse goes from where it pressed, whatever the direction.
 * Either acts on `steps` only; the other bars keep their values.
 *
 * `bottom` is the value at the bottom of the bars and `top` the one at the
 * top: a row drawn upside down passes them the other way round.
 */
struct BarsDrag
{
  enum class Kind
  {
    None,
    Offset,
    Random
  };

  Kind kind{Kind::None};
  QPointF origin;
  std::vector<float> base;
  //! For Random: where each bar ends up when dragged all the way.
  std::vector<float> target;
  std::vector<int> steps;

  explicit operator bool() const noexcept { return kind != Kind::None; }

  void start(
      Kind k, QPointF pos, std::vector<float> values, std::vector<float> targets,
      std::vector<int> affected)
  {
    kind = k;
    origin = pos;
    base = std::move(values);
    target = std::move(targets);
    target.resize(base.size());
    steps = std::move(affected);
  }

  void stop() noexcept { kind = Kind::None; }

  //! The values with the mouse at `pos`, over bars `height` high.
  std::vector<float> values(QPointF pos, double height, float bottom, float top) const
  {
    std::vector<float> res = base;
    if(height <= 0.)
      return res;
    const float lo = std::min(bottom, top);
    const float hi = std::max(bottom, top);
    if(kind == Kind::Offset)
    {
      const float delta = float((origin.y() - pos.y()) / height) * (top - bottom);
      for(int i : steps)
        if(i >= 0 && i < std::ssize(res))
          res[i] = std::clamp(base[i] + delta, lo, hi);
    }
    else if(kind == Kind::Random)
    {
      const QPointF d = pos - origin;
      const float t = std::clamp(
          float(std::hypot(d.x(), d.y()) / std::max(80., height)), 0.f, 1.f);
      for(int i : steps)
        if(i >= 0 && i < std::ssize(res))
          res[i] = base[i] + t * (target[i] - base[i]);
    }
    return res;
  }
};
}
