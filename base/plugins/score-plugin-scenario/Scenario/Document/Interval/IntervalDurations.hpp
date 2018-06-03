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

  double speed() const
  {
    return m_speed;
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
  void setSpeed(double Speed)
  {
    if (m_speed == Speed)
      return;

    m_speed = Speed;
    speedChanged(Speed);
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
  void speedChanged(double speed) W_SIGNAL(speedChanged, speed);
  void guiDurationChanged(TimeVal guiDuration) W_SIGNAL(guiDurationChanged, guiDuration);

  PROPERTY(double, speed READ speed WRITE setSpeed NOTIFY speedChanged, W_Final)
  PROPERTY(bool, isMaxInfinite READ isMaxInfinite WRITE setMaxInfinite NOTIFY maxInfiniteChanged, W_Final)
  PROPERTY(bool, isMinNull READ isMinNull WRITE setMinNull NOTIFY minNullChanged, W_Final)
  PROPERTY(bool, isRigid READ isRigid WRITE setRigid NOTIFY rigidityChanged, W_Final)
  PROPERTY(double, percentage READ playPercentage WRITE setPlayPercentage NOTIFY playPercentageChanged, W_Final)
  PROPERTY(TimeVal, guiDuration READ guiDuration WRITE setGuiDuration NOTIFY guiDurationChanged, W_Final)
  PROPERTY(TimeVal, max READ maxDuration WRITE setMaxDuration NOTIFY maxDurationChanged, W_Final)
  PROPERTY(TimeVal, min READ minDuration WRITE setMinDuration NOTIFY minDurationChanged, W_Final)
  PROPERTY(TimeVal, default READ defaultDuration WRITE setDefaultDuration NOTIFY defaultDurationChanged, W_Final)
private:
  IntervalModel& m_model;

  TimeVal m_defaultDuration{std::chrono::milliseconds{200}};
  TimeVal m_minDuration{m_defaultDuration};
  TimeVal m_maxDuration{m_defaultDuration};
  TimeVal m_guiDuration{m_defaultDuration};

  double m_playPercentage{}; // Between 0 and 1.
  double m_speed{1};
  bool m_rigidity{true};
  bool m_isMinNull{false};
  bool m_isMaxInfinite{false};
};
}
