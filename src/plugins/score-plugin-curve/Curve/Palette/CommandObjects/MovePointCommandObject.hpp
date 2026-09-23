#pragma once
#include "CurveCommandObjectBase.hpp"

#include <Curve/Segment/CurveSegmentData.hpp>

#include <ossia/detail/small_vector.hpp>

namespace Curve
{
class Presenter;
class SCORE_PLUGIN_CURVE_EXPORT MovePointCommandObject final : public CommandObjectBase
{
public:
  MovePointCommandObject(
      const Model& model, Presenter* presenter, const score::CommandStackFacade& stack);
  ~MovePointCommandObject();

  void on_press() override;

  void move();

  void release();

  void cancel();

private:
  bool crosses(double x) const;
  void setCurrentPoint(std::vector<SegmentData>& segments);
  bool suppressOverlapped(std::vector<SegmentData>& segments) const;
  bool crossOverlapped(std::vector<SegmentData>& segments) const;
  void setTooltip(const Curve::Point& p);
  void unsetTooltip();

  bool m_pressed{};
  // In m_startSegments, found at press.
  int64_t m_prevIndex{-1}, m_follIndex{-1};
  // Where m_segments differs from m_startSegments after a plain move.
  ossia::small_vector<int64_t, 2> m_touched;
  // The nearest points on each side at press, whatever the locking.
  double m_innerMin{}, m_innerMax{};
};
}
