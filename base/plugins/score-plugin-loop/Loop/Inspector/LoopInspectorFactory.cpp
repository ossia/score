// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Inspector/InspectorWidgetList.hpp>
#include <Loop/LoopProcessModel.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegateFactory.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorWidget.hpp>

#include <score/tools/std/Optional.hpp>

#include <QByteArray>
#include <QDataStream>
#include <QObject>
#include <QtGlobal>
#include <algorithm>
#include <list>

#include "LoopInspectorFactory.hpp"
#include "LoopInspectorWidget.hpp"
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <Process/ExpandMode.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Commands/MoveBaseEvent.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegate.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/OngoingCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <Process/TimeValueSerialization.hpp>

class InspectorWidgetBase;
class QWidget;

// TODO cleanup this file
namespace Loop
{
class IntervalInspectorDelegate final
    : public Scenario::IntervalInspectorDelegate
{
public:
  IntervalInspectorDelegate(const Scenario::IntervalModel& cst);

  void updateElements() override;
  void addWidgets_pre(
      std::vector<QWidget*>& widgets,
      Scenario::IntervalInspectorWidget* parent) override;
  void addWidgets_post(
      std::vector<QWidget*>& widgets,
      Scenario::IntervalInspectorWidget* parent) override;

  void on_defaultDurationChanged(
      OngoingCommandDispatcher& dispatcher,
      const TimeVal& val,
      ExpandMode) const override;
};

IntervalInspectorDelegate::IntervalInspectorDelegate(
    const Scenario::IntervalModel& cst)
    : Scenario::IntervalInspectorDelegate{cst}
{
}

void IntervalInspectorDelegate::updateElements()
{
}

void IntervalInspectorDelegate::addWidgets_pre(
    std::vector<QWidget*>& widgets, Scenario::IntervalInspectorWidget* parent)
{
}

void IntervalInspectorDelegate::addWidgets_post(
    std::vector<QWidget*>& widgets, Scenario::IntervalInspectorWidget* parent)
{
}

void IntervalInspectorDelegate::on_defaultDurationChanged(
    OngoingCommandDispatcher& dispatcher,
    const TimeVal& val,
    ExpandMode expandmode) const
{
  auto& loop = *safe_cast<Loop::ProcessModel*>(m_model.parent());
  dispatcher
      .submitCommand<Scenario::Command::MoveBaseEvent<Loop::ProcessModel>>(
          loop,
          loop.state(m_model.endState()).eventId(),
          m_model.date() + val,
          0,
          expandmode, LockMode::Free);
}

IntervalInspectorDelegateFactory::~IntervalInspectorDelegateFactory()
{
}

std::unique_ptr<Scenario::IntervalInspectorDelegate>
IntervalInspectorDelegateFactory::make(
    const Scenario::IntervalModel& interval)
{
  return std::make_unique<Loop::IntervalInspectorDelegate>(interval);
}

bool IntervalInspectorDelegateFactory::matches(
    const Scenario::IntervalModel& interval) const
{
  return dynamic_cast<Loop::ProcessModel*>(interval.parent());
}
}
