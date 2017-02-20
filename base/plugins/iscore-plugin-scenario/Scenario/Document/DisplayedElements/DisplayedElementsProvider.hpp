#pragma once
#include <Scenario/Document/DisplayedElements/DisplayedElementsContainer.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

class QQuickPaintedItem;
namespace Process
{
struct ProcessPresenterContext;
}
namespace Scenario
{
class ConstraintModel;

class ISCORE_PLUGIN_SCENARIO_EXPORT DisplayedElementsProvider
    : public iscore::Interface<DisplayedElementsProvider>
{
  ISCORE_INTERFACE("4bfcf0ee-6c47-405a-a15d-9da73436e273")
public:
  virtual ~DisplayedElementsProvider();
  virtual bool matches(const ConstraintModel& cst) const = 0;
  bool matches(
      const ConstraintModel& cst,
      const Process::ProcessPresenterContext& ctx,
      QQuickPaintedItem* view_parent,
      QObject* parent) const
  {
    return matches(cst);
  }

  virtual DisplayedElementsContainer make(ConstraintModel& cst) const = 0;
  virtual DisplayedElementsPresenterContainer make_presenters(
      const ConstraintModel& m,
      const Process::ProcessPresenterContext& ctx,
      QQuickPaintedItem* view_parent,
      QObject* parent) const = 0;
};
}
