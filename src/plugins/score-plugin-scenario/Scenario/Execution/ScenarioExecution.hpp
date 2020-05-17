#pragma once
#include <Scenario/Palette/ScenarioPoint.hpp>

#include <score/model/Identifier.hpp>

#include <QObject>

#include <score_plugin_scenario_export.h>

#include <verdigris>
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
  ScenarioExecution();
  ~ScenarioExecution();
  //! Play a single state
  void playState(const ScenarioInterface& arg_1, Id<StateModel> arg_2)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, playState, arg_1, arg_2)

  //! Play a single IntervalModel
  void playInterval(const ScenarioInterface& arg_1, Id<IntervalModel> arg_2)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, playInterval, arg_1, arg_2)

  /**
   * @brief Play from a given point in a IntervalModel.
   *
   * The other branches of the Scenario will be discarded, i.e. all the
   * IntervalModel that aren't originating from this one.
   */
  void playFromIntervalAtDate(
      const ScenarioInterface& arg_1,
      Id<IntervalModel> arg_2,
      const TimeVal& arg_3)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, playFromIntervalAtDate, arg_1, arg_2, arg_3)

  //! "Play from here" algorithm.
  void playAtDate(const TimeVal& arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, playAtDate, arg_1)

  void transport(const TimeVal& arg_1) E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, transport, arg_1)

  //! Request an automation recording from a given point.
  void startRecording(ProcessModel& arg_1, Point arg_2)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, startRecording, arg_1, arg_2)

  //! Request a message recording from a given point.
  void startRecordingMessages(ProcessModel& arg_1, Point arg_2)
      E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, startRecordingMessages, arg_1, arg_2)

  void stopRecording() E_SIGNAL(SCORE_PLUGIN_SCENARIO_EXPORT, stopRecording)
};
}
