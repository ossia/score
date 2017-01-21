#pragma once
#include <QObject>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>
namespace Scenario
{
class ScenarioInterface;
class ProcessModel;
class StateModel;
class ConstraintModel;

/**
 * @brief API for the various elements that we can execute.
 *
 * This is used for instance by the "Play" tool, PlayToolState.
 */
class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioExecution : public QObject
{
  Q_OBJECT
signals:
  //! Play a single state
  void playState(
      const Scenario::ScenarioInterface&, Id<Scenario::StateModel>) const;

  //! Play a single ConstraintModel
  void playConstraint(
      const Scenario::ScenarioInterface&, Id<Scenario::ConstraintModel>) const;

  /**
   * @brief Play from a given point in a ConstraintModel.
   *
   * The other branches of the Scenario will be discarded, i.e. all the ConstraintModel
   * that aren't originating from this one.
   */
  void playFromConstraintAtDate(
      const Scenario::ScenarioInterface&,
      Id<Scenario::ConstraintModel>,
      const TimeVal&) const;

  //! "Play from here" algorithm.
  void playAtDate(const TimeVal&) const;

  //! Request an automation recording from a given point.
  void startRecording(const Scenario::ProcessModel&, Scenario::Point) const;

  //! Request a message recording from a given point.
  void
  startRecordingMessages(const Scenario::ProcessModel&, Scenario::Point) const;
  void stopRecording() const;
};
}
