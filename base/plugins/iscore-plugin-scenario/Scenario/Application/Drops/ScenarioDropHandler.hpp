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
  virtual bool handle(
      const Scenario::TemporalScenarioPresenter&,
      QPointF pos,
      const QMimeData* mime)
      = 0;
};

class DropHandlerList final : public iscore::InterfaceList<DropHandler>
{
public:
  virtual ~DropHandlerList();

  bool handle(
      const TemporalScenarioPresenter& scen,
      QPointF pos,
      const QMimeData* mime) const;
};

class ConstraintModel;
class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintDropHandler
    : public iscore::Interface<ConstraintDropHandler>
{
  ISCORE_INTERFACE("b9f3efc0-b906-487a-ac49-87924edd2cff")
public:
  virtual ~ConstraintDropHandler();

  // Returns false if not handled.
  virtual bool handle(const Scenario::ConstraintModel&, const QMimeData* mime)
      = 0;
};

class ConstraintDropHandlerList final
    : public iscore::InterfaceList<ConstraintDropHandler>
{
public:
  virtual ~ConstraintDropHandlerList();

  bool handle(const Scenario::ConstraintModel&, const QMimeData* mime) const;
};
}
