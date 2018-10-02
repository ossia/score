#pragma once
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/FactoryInterface.hpp>

#include <QMimeData>
#include <QPointF>

#include <score_plugin_scenario_export.h>

namespace Scenario
{
class ScenarioPresenter;
class SCORE_PLUGIN_SCENARIO_EXPORT DropHandler : public score::InterfaceBase
{
  SCORE_INTERFACE(DropHandler, "ce1c5b6c-fe4c-416f-877c-eae642a1413a")
public:
  ~DropHandler() override;

  // Returns false if not handled.
  virtual bool dragEnter(
      const Scenario::ScenarioPresenter&, QPointF pos,
      const QMimeData& mime)
  {
    return false;
  }
  virtual bool dragMove(
      const Scenario::ScenarioPresenter&, QPointF pos,
      const QMimeData& mime)
  {
    return false;
  }
  virtual bool dragLeave(
      const Scenario::ScenarioPresenter&, QPointF pos,
      const QMimeData& mime)
  {
    return false;
  }
  virtual bool drop(
      const Scenario::ScenarioPresenter&, QPointF pos,
      const QMimeData& mime)
      = 0;
};

class DropHandlerList final : public score::InterfaceList<DropHandler>
{
public:
  ~DropHandlerList() override;

  bool dragEnter(
      const ScenarioPresenter& scen, QPointF pos,
      const QMimeData& mime) const;
  bool dragMove(
      const ScenarioPresenter& scen, QPointF pos,
      const QMimeData& mime) const;
  bool dragLeave(
      const ScenarioPresenter&, QPointF pos,
      const QMimeData& mime) const;
  bool drop(
      const ScenarioPresenter& scen, QPointF pos,
      const QMimeData& mime) const;
};

class IntervalModel;
class SCORE_PLUGIN_SCENARIO_EXPORT IntervalDropHandler
    : public score::InterfaceBase
{
  SCORE_INTERFACE(IntervalDropHandler, "b9f3efc0-b906-487a-ac49-87924edd2cff")
public:
  ~IntervalDropHandler() override;

  // Returns false if not handled.
  virtual bool drop(const Scenario::IntervalModel&, const QMimeData& mime) = 0;
};

class IntervalDropHandlerList final
    : public score::InterfaceList<IntervalDropHandler>
{
public:
  ~IntervalDropHandlerList() override;

  bool drop(const Scenario::IntervalModel&, const QMimeData& mime) const;
};
}
