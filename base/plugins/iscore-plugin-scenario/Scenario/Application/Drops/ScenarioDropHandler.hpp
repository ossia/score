#pragma once
#include <QMimeData>
#include <QPointF>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class TemporalScenarioPresenter;
class ISCORE_PLUGIN_SCENARIO_EXPORT DropHandler
    : public iscore::Interface<DropHandler>
{
  ISCORE_INTERFACE("ce1c5b6c-fe4c-416f-877c-eae642a1413a")
public:
  virtual ~DropHandler();

  // Returns false if not handled.
  virtual bool dragEnter(
      const Scenario::TemporalScenarioPresenter&,
      QPointF pos,
      const QMimeData* mime) { return false; }
  virtual bool dragMove(
      const Scenario::TemporalScenarioPresenter&,
      QPointF pos,
      const QMimeData* mime) { return false; }
  virtual bool dragLeave(
      const Scenario::TemporalScenarioPresenter&,
      QPointF pos,
      const QMimeData* mime) { return false; }
  virtual bool drop(
      const Scenario::TemporalScenarioPresenter&,
      QPointF pos,
      const QMimeData* mime)
      = 0;
};

class DropHandlerList final : public iscore::InterfaceList<DropHandler>
{
public:
  virtual ~DropHandlerList();

  bool dragEnter(
      const TemporalScenarioPresenter& scen,
      QPointF pos,
      const QMimeData* mime) const;
  bool dragMove(
      const TemporalScenarioPresenter& scen,
      QPointF pos,
      const QMimeData* mime) const;
  bool dragLeave(
      const TemporalScenarioPresenter&,
      QPointF pos,
      const QMimeData* mime) const;
  bool drop(
      const TemporalScenarioPresenter& scen,
      QPointF pos,
      const QMimeData* mime) const;
};

class IntervalModel;
class ISCORE_PLUGIN_SCENARIO_EXPORT IntervalDropHandler
    : public iscore::Interface<IntervalDropHandler>
{
  ISCORE_INTERFACE("b9f3efc0-b906-487a-ac49-87924edd2cff")
public:
  virtual ~IntervalDropHandler();

  // Returns false if not handled.
  virtual bool drop(const Scenario::IntervalModel&, const QMimeData* mime)
      = 0;
};

class IntervalDropHandlerList final
    : public iscore::InterfaceList<IntervalDropHandler>
{
public:
  virtual ~IntervalDropHandlerList();

  bool drop(const Scenario::IntervalModel&, const QMimeData* mime) const;
};
}
