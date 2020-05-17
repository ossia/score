#pragma once
#include "CurveCommandObjectBase.hpp"

#include <Curve/Segment/CurveSegmentData.hpp>

namespace Curve
{
struct CurveSegmentMap;

class Presenter;
class SCORE_PLUGIN_CURVE_EXPORT MovePointCommandObject final : public CommandObjectBase
{
public:
  MovePointCommandObject(Presenter* presenter, const score::CommandStackFacade& stack);
  ~MovePointCommandObject();

  void on_press() override;

  void move();

  void release();

  void cancel();

private:
  void handlePointOverlap(CurveSegmentMap& segments);
  void handleSuppressOnOverlap(CurveSegmentMap& segments);
  void handleCrossOnOverlap(CurveSegmentMap& segments);
  void setCurrentPoint(CurveSegmentMap& segments);
};
}
