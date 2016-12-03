#pragma once
#include <Loop/Inspector/LoopInspectorWidget.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>
#include <QList>
#include <QString>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegateFactory.hpp>
#include <memory>

class InspectorWidgetBase;
class QObject;
class QWidget;
namespace iscore
{
class Document;
} // namespace iscore

namespace Scenario
{
class ConstraintInspectorDelegate;
class ConstraintModel;
}
namespace Loop
{
// TODO Clean this file
class ConstraintInspectorDelegateFactory
    : public Scenario::ConstraintInspectorDelegateFactory
{
  ISCORE_CONCRETE_FACTORY("295245d4-2019-4849-9d49-10c1e21c209c")
public:
  virtual ~ConstraintInspectorDelegateFactory();

private:
  std::unique_ptr<Scenario::ConstraintInspectorDelegate>
  make(const Scenario::ConstraintModel& constraint) override;

  bool matches(const Scenario::ConstraintModel& constraint) const override;
};

class InspectorFactory final
    : public Process::
          InspectorWidgetDelegateFactory_T<ProcessModel, LoopInspectorWidget>
{
  ISCORE_CONCRETE_FACTORY("f45f98f2-f721-4ffa-9219-114832fe06bd")
};
}
