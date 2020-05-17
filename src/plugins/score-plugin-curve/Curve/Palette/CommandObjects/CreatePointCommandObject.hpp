#pragma once
#include <Curve/Palette/CommandObjects/CurveCommandObjectBase.hpp>

#include <vector>

namespace score
{
class CommandStackFacade;
}

namespace Curve
{
struct SegmentData;
class Presenter;

class SCORE_PLUGIN_CURVE_EXPORT CreatePointCommandObject final : public CommandObjectBase
{
public:
  CreatePointCommandObject(Presenter* presenter, const score::CommandStackFacade& stack);
  virtual ~CreatePointCommandObject();

  void on_press() override;

  void move();

  void release();

  void cancel();

private:
  void createPoint(std::vector<SegmentData>& segments);
};
}
