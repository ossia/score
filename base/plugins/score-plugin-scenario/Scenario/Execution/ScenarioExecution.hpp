#pragma once
#include <QObject>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <score/model/Identifier.hpp>
#include <score_plugin_scenario_export.h>
namespace Scenario
{
class ScenarioInterface;
class ProcessModel;
class StateModel;
class IntervalModel;

/**
 * @brief API for the various elements that we can execute.
 *
 * This is used for instance by the "Play" tool, PlayToolState.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT ScenarioExecution : public QObject
{
  Q_OBJECT
Q_SIGNALS:
  //! Play a single state
  void playState(
      const Scenario::ScenarioInterface&, Id<Scenario::StateModel>) const;

  //! Play a single IntervalModel
  void playInterval(
      const Scenario::ScenarioInterface&, Id<Scenario::IntervalModel>) const;

  /**
   * @brief Play from a given point in a IntervalModel.
   *
   * The other branches of the Scenario will be discarded, i.e. all the IntervalModel
   * that aren't originating from this one.
   */
  void playFromIntervalAtDate(
      const Scenario::ScenarioInterface&,
      Id<Scenario::IntervalModel>,
      const TimeVal&) const;

  //! "Play from here" algorithm.
  void playAtDate(const TimeVal&) const;

  //! Request an automation recording from a given point.
  void startRecording(Scenario::ProcessModel&, Scenario::Point) const;

  //! Request a message recording from a given point.
  void
  startRecordingMessages(Scenario::ProcessModel&, Scenario::Point) const;
  void stopRecording() const;
};
}
