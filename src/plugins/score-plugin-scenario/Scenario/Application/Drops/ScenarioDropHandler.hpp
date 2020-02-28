#pragma once
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <QMimeData>
#include <QPointF>

#include <score_plugin_scenario_export.h>


namespace score
{
struct DocumentContext;
}
namespace Scenario
{
class StateModel;
class ProcessModel;
class ScenarioPresenter;

struct MagneticStates
{
  Scenario::StateModel* horizontal{};
  Scenario::StateModel* vertical{};
  bool magnetic{};
};

MagneticStates
magneticStates(
    MagneticStates cur, Scenario::Point pt, const Scenario::ScenarioPresenter& pres);


class ScenarioPresenter;
class SCORE_PLUGIN_SCENARIO_EXPORT DropHandler : public score::InterfaceBase
{
  SCORE_INTERFACE(DropHandler, "ce1c5b6c-fe4c-416f-877c-eae642a1413a")
public:
  ~DropHandler() override;

  // Returns false if not handled.
  virtual bool dragEnter(
      const Scenario::ScenarioPresenter&,
      QPointF pos,
      const QMimeData& mime);

  virtual bool dragMove(
      const Scenario::ScenarioPresenter&,
      QPointF pos,
      const QMimeData& mime);

  virtual bool dragLeave(
      const Scenario::ScenarioPresenter&,
      QPointF pos,
      const QMimeData& mime);

  virtual bool canDrop(const QMimeData& mime) const noexcept;

  virtual bool
  drop(const Scenario::ScenarioPresenter&, QPointF pos, const QMimeData& mime)
      = 0;
};

class SCORE_PLUGIN_SCENARIO_EXPORT GhostIntervalDropHandler : public DropHandler
{
public:
  ~GhostIntervalDropHandler() override;

protected:
  std::vector<QString> m_acceptableMimeTypes;
  std::vector<QString> m_acceptableSuffixes;

  MagneticStates m_magnetic{};

private:
  bool dragEnter(
      const Scenario::ScenarioPresenter&,
      QPointF pos,
      const QMimeData& mime) final override;
  bool dragMove(
      const Scenario::ScenarioPresenter&,
      QPointF pos,
      const QMimeData& mime) final override;
  bool dragLeave(
      const Scenario::ScenarioPresenter&,
      QPointF pos,
      const QMimeData& mime) final override;
};

class DropHandlerList final : public score::InterfaceList<DropHandler>
{
public:
  ~DropHandlerList() override;

  bool dragEnter(
      const ScenarioPresenter& scen,
      QPointF pos,
      const QMimeData& mime) const;
  bool dragMove(
      const ScenarioPresenter& scen,
      QPointF pos,
      const QMimeData& mime) const;
  bool dragLeave(const ScenarioPresenter&, QPointF pos, const QMimeData& mime)
      const;
  bool drop(const ScenarioPresenter& scen, QPointF pos, const QMimeData& mime)
      const;
};

class IntervalModel;
class SCORE_PLUGIN_SCENARIO_EXPORT IntervalDropHandler
    : public score::InterfaceBase
{
  SCORE_INTERFACE(IntervalDropHandler, "b9f3efc0-b906-487a-ac49-87924edd2cff")
public:
  ~IntervalDropHandler() override;

  // Returns false if not handled.
  virtual bool drop(
      const score::DocumentContext& ctx,
      const Scenario::IntervalModel&,
      QPointF pos,
      const QMimeData& mime)
      = 0;
};

class IntervalDropHandlerList final
    : public score::InterfaceList<IntervalDropHandler>
{
public:
  ~IntervalDropHandlerList() override;

  bool drop(
      const score::DocumentContext& ctx,
      const Scenario::IntervalModel&,
      QPointF pos,
      const QMimeData& mime) const;
};
}
