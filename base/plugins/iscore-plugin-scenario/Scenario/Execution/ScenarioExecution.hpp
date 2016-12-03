#pragma once
#include <QObject>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Scenario
{
class ScenarioInterface;
class ProcessModel;
class StateModel;
class ConstraintModel;
class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioExecution : public QObject
{
  Q_OBJECT
signals:
  void playState(
      const Scenario::ScenarioInterface&, Id<Scenario::StateModel>) const;
  void playConstraint(
      const Scenario::ScenarioInterface&, Id<Scenario::ConstraintModel>) const;
  void playAtDate(const TimeValue&) const;

  void startRecording(const Scenario::ProcessModel&, Scenario::Point) const;
  void
  startRecordingMessages(const Scenario::ProcessModel&, Scenario::Point) const;
  void stopRecording() const;
};
}
