#pragma once
#include <QMimeData>
#include <QPointF>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/FactoryInterface.hpp>
#include <score_plugin_scenario_export.h>

namespace Scenario
{
class TemporalScenarioPresenter;
class SCORE_PLUGIN_SCENARIO_EXPORT DropHandler
    : public score::Interface<DropHandler>
{
  SCORE_INTERFACE("ce1c5b6c-fe4c-416f-877c-eae642a1413a")
public:
  virtual ~DropHandler();

  // Returns false if not handled.
  virtual bool dragEnter(
      const Scenario::TemporalScenarioPresenter&,
      QPointF pos,
      const QMimeData* mime)
  {
    return false;
  }
  virtual bool dragMove(
      const Scenario::TemporalScenarioPresenter&,
      QPointF pos,
      const QMimeData* mime)
  {
    return false;
  }
  virtual bool dragLeave(
      const Scenario::TemporalScenarioPresenter&,
      QPointF pos,
      const QMimeData* mime)
  {
    return false;
  }
  virtual bool drop(
      const Scenario::TemporalScenarioPresenter&,
      QPointF pos,
      const QMimeData* mime)
      = 0;
};

class DropHandlerList final : public score::InterfaceList<DropHandler>
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
class SCORE_PLUGIN_SCENARIO_EXPORT IntervalDropHandler
    : public score::Interface<IntervalDropHandler>
{
  SCORE_INTERFACE("b9f3efc0-b906-487a-ac49-87924edd2cff")
public:
  virtual ~IntervalDropHandler();

  // Returns false if not handled.
  virtual bool drop(const Scenario::IntervalModel&, const QMimeData* mime) = 0;
};

class IntervalDropHandlerList final
    : public score::InterfaceList<IntervalDropHandler>
{
public:
  virtual ~IntervalDropHandlerList();

  bool drop(const Scenario::IntervalModel&, const QMimeData* mime) const;
};
}
