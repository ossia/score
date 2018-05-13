#pragma once
#include <Process/TimeValue.hpp>
#include <wobjectdefs.h>
#include <QObject>
#include <chrono>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score_plugin_scenario_export.h>

class DataStream;
class JSONObject;

namespace Scenario
{
class IntervalModel;

// A container class to separate management of the duration of a interval.
class SCORE_PLUGIN_SCENARIO_EXPORT IntervalDurations final : public QObject
{
  // These dates are relative to the beginning of the interval.


  SCORE_SERIALIZE_FRIENDS

  W_OBJECT(IntervalDurations)
public:
  IntervalDurations(IntervalModel& model) : m_model{model}
  {
  }

  ~IntervalDurations();

  IntervalDurations& operator=(const IntervalDurations& other);

  const TimeVal& defaultDuration() const
  {
    return m_defaultDuration;
  }

  TimeVal minDuration() const
  {
    if (m_isMinNull)
      return TimeVal::zero();
    return m_minDuration;
  }

  TimeVal maxDuration() const
  {
    if (m_isMaxInfinite)
      return PositiveInfinity{};
    return m_maxDuration;
  }

  double playPercentage() const
  {
    return m_playPercentage;
  }

  double executionSpeed() const
  {
    return m_executionSpeed;
  }

  bool isRigid() const
  {
    return m_rigidity;
  }

  TimeVal guiDuration() const
  {
    return m_guiDuration;
  }

  bool isMinNull() const
  {
    return m_isMinNull;
  }

  bool isMaxInfinite() const
  {
    return m_isMaxInfinite;
  }

  void setDefaultDuration(const TimeVal& arg);
  void setMinDuration(const TimeVal& arg);
  void setMaxDuration(const TimeVal& arg);
  void setGuiDuration(TimeVal guiDuration);

  void setPlayPercentage(double arg);
  void setRigid(bool arg);
  void setMinNull(bool isMinNull);
  void setMaxInfinite(bool isMaxInfinite);
  void setExecutionSpeed(double executionSpeed)
  {
    if (m_executionSpeed == executionSpeed)
      return;

    m_executionSpeed = executionSpeed;
    executionSpeedChanged(executionSpeed);
  }

  void checkConsistency();

  // Modification algorithms that keep everything consistent
  class SCORE_PLUGIN_SCENARIO_EXPORT Algorithms
  {
  public:
    static void setDurationInBounds(IntervalModel& cstr, const TimeVal& time);
    static void changeAllDurations(IntervalModel& cstr, const TimeVal& time);
    static void scaleAllDurations(IntervalModel& cstr, const TimeVal& time);
  };

public:
  void defaultDurationChanged(const TimeVal& arg) W_SIGNAL(defaultDurationChanged, arg);
  void minDurationChanged(const TimeVal& arg) W_SIGNAL(minDurationChanged, arg);
  void maxDurationChanged(const TimeVal& arg) W_SIGNAL(maxDurationChanged, arg);
  void playPercentageChanged(double arg) W_SIGNAL(playPercentageChanged, arg);
  void rigidityChanged(bool arg) W_SIGNAL(rigidityChanged, arg);
  void minNullChanged(bool isMinNull) W_SIGNAL(minNullChanged, isMinNull);
  void maxInfiniteChanged(bool isMaxInfinite) W_SIGNAL(maxInfiniteChanged, isMaxInfinite);
  void executionSpeedChanged(double executionSpeed) W_SIGNAL(executionSpeedChanged, executionSpeed);
  void guiDurationChanged(TimeVal guiDuration) W_SIGNAL(guiDurationChanged, guiDuration);

private:
  IntervalModel& m_model;

  TimeVal m_defaultDuration{std::chrono::milliseconds{200}};
  TimeVal m_minDuration{m_defaultDuration};
  TimeVal m_maxDuration{m_defaultDuration};
  TimeVal m_guiDuration{m_defaultDuration};

  double m_playPercentage{}; // Between 0 and 1.
  double m_executionSpeed{1};
  bool m_rigidity{true};
  bool m_isMinNull{false};
  bool m_isMaxInfinite{false};

W_PROPERTY(double, executionSpeed READ executionSpeed WRITE setExecutionSpeed NOTIFY executionSpeedChanged, W_Final)

W_PROPERTY(bool, isMaxInfinite READ isMaxInfinite WRITE setMaxInfinite NOTIFY maxInfiniteChanged, W_Final)

W_PROPERTY(bool, isMinNull READ isMinNull WRITE setMinNull NOTIFY minNullChanged, W_Final)

W_PROPERTY(bool, isRigid READ isRigid WRITE setRigid NOTIFY rigidityChanged, W_Final)

W_PROPERTY(double, playPercentage READ playPercentage WRITE setPlayPercentage NOTIFY playPercentageChanged, W_Final)

W_PROPERTY(TimeVal, guiDuration READ guiDuration WRITE setGuiDuration NOTIFY guiDurationChanged, W_Final)

W_PROPERTY(TimeVal, maxDuration READ maxDuration WRITE setMaxDuration NOTIFY maxDurationChanged, W_Final)

W_PROPERTY(TimeVal, minDuration READ minDuration WRITE setMinDuration NOTIFY minDurationChanged, W_Final)
};
}
