#pragma once
#include "CurveCommandObjectBase.hpp"

#include <Curve/Segment/CurveSegmentData.hpp>

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
  bool setCurrentPoint(std::vector<SegmentData>& segments) const;
  bool suppressOverlapped(std::vector<SegmentData>& segments) const;
  bool crossOverlapped(std::vector<SegmentData>& segments) const;
  void setTooltip(const Curve::Point& p);
  void unsetTooltip();

  bool m_pressed{};
};
}
