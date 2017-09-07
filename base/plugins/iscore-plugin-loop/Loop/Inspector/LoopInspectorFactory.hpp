#pragma once
#include <Loop/Inspector/LoopInspectorWidget.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <QList>
#include <QString>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegateFactory.hpp>
#include <memory>

namespace Scenario
{
class IntervalInspectorDelegate;
class IntervalModel;
}
namespace Loop
{
// TODO Clean this file
class IntervalInspectorDelegateFactory
    : public Scenario::IntervalInspectorDelegateFactory
{
  ISCORE_CONCRETE("295245d4-2019-4849-9d49-10c1e21c209c")
public:
  virtual ~IntervalInspectorDelegateFactory();

private:
  std::unique_ptr<Scenario::IntervalInspectorDelegate>
  make(const Scenario::IntervalModel& interval) override;

  bool matches(const Scenario::IntervalModel& interval) const override;
};

class InspectorFactory final
    : public Process::
          InspectorWidgetDelegateFactory_T<ProcessModel, LoopInspectorWidget>
{
  ISCORE_CONCRETE("f45f98f2-f721-4ffa-9219-114832fe06bd")
};
}
