#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>

#include <score/plugins/Interface.hpp>

class QGraphicsItem;
namespace Process
{
struct Context;
}
namespace Scenario
{
class IntervalModel;

class SCORE_PLUGIN_SCENARIO_EXPORT DisplayedElementsProvider : public score::InterfaceBase
{
  SCORE_INTERFACE(DisplayedElementsProvider, "4bfcf0ee-6c47-405a-a15d-9da73436e273")
public:
  virtual ~DisplayedElementsProvider();
  virtual bool matches(const IntervalModel& cst) const = 0;
  bool matches(
      ZoomRatio zoom,
      const IntervalModel& cst,
      const Process::Context& ctx,
      QGraphicsItem* view_parent,
      QObject* parent) const
  {
    return matches(cst);
  }

  virtual DisplayedElementsContainer make(IntervalModel& cst) const = 0;
  virtual DisplayedElementsPresenterContainer make_presenters(
      ZoomRatio zoom,
      const IntervalModel& m,
      const Process::Context& ctx,
      QGraphicsItem* view_parent,
      QObject* parent) const = 0;
};
}
