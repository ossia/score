#pragma once
#include <score/plugins/Interface.hpp>
#include <score/statemachine/GraphicsSceneToolPalette.hpp>

#include <score_plugin_scenario_export.h>

#include <memory>
class QGraphicsItem;
namespace Scenario
{
class IntervalModel;
class ScenarioDocumentPresenter;

class SCORE_PLUGIN_SCENARIO_EXPORT DisplayedElementsToolPaletteFactory
    : public score::InterfaceBase
{
  SCORE_INTERFACE(DisplayedElementsToolPaletteFactory, "0884edb9-81e4-44ff-856f-fbc772f6d683")
public:
  virtual ~DisplayedElementsToolPaletteFactory();

  virtual bool matches(const IntervalModel& interval) const = 0;

  bool
  matches(ScenarioDocumentPresenter& pres, const IntervalModel& interval, QGraphicsItem*) const
  {
    return matches(interval);
  }

  virtual std::unique_ptr<GraphicsSceneToolPalette>
  make(ScenarioDocumentPresenter& pres, const IntervalModel& interval, QGraphicsItem* parent) = 0;
};
}
