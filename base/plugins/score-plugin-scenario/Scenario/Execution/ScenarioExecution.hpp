#pragma once
#include <QObject>
#include <wobjectdefs.h>
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
  W_OBJECT(ScenarioExecution)
public:
  //! Play a single state
  void playState(
      const Scenario::ScenarioInterface& arg_1, Id<Scenario::StateModel> arg_2) const W_SIGNAL(playState, arg_1, arg_2);

  //! Play a single IntervalModel
  void playInterval(
      const Scenario::ScenarioInterface& arg_1, Id<Scenario::IntervalModel>  arg_2) const W_SIGNAL(playInterval, arg_1, arg_2);

  /**
   * @brief Play from a given point in a IntervalModel.
   *
   * The other branches of the Scenario will be discarded, i.e. all the
   * IntervalModel that aren't originating from this one.
   */
  void playFromIntervalAtDate(
      const Scenario::ScenarioInterface& arg_1,
      Id<Scenario::IntervalModel> arg_2,
      const TimeVal&  arg_3) const W_SIGNAL(playFromIntervalAtDate, arg_1, arg_2, arg_3);

  //! "Play from here" algorithm.
  void playAtDate(const TimeVal& arg_1) const W_SIGNAL(playAtDate, arg_1);

  void transport(const TimeVal& arg_1) const W_SIGNAL(transport, arg_1);

  //! Request an automation recording from a given point.
  void startRecording(Scenario::ProcessModel& arg_1, Scenario::Point arg_2) const W_SIGNAL(startRecording, arg_1, arg_2);

  //! Request a message recording from a given point.
  void startRecordingMessages(Scenario::ProcessModel& arg_1, Scenario::Point arg_2) const W_SIGNAL(startRecordingMessages, arg_1, arg_2);
  void stopRecording() const W_SIGNAL(stopRecording);
};
}
