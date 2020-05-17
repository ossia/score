#pragma once
#include <Process/TimeValue.hpp>

#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>

#include <ossia/editor/scenario/time_value.hpp>

#include <Execution/Clock/ClockFactory.hpp>
#include <score_plugin_engine_export.h>

#include <verdigris>

namespace Execution
{
namespace Settings
{
struct SchedulingPolicies
{
  const QString StaticFixed{"Static (Plain)"};
  const QString StaticBFS{"Static (BFS)"};
  const QString StaticTC{"Static (TC)"};
  const QString Dynamic{"Dynamic"};
  operator QStringList() const { return {StaticFixed, StaticBFS, StaticTC, Dynamic}; }
};
struct OrderingPolicies
{
  const QString CreationOrder{"Creation order"};
  const QString XY{"XY"};
  const QString YX{"YX"};
  const QString Temporal{"Temporal"};
  operator QStringList() const { return {CreationOrder, XY, YX, Temporal}; }
};
struct MergingPolicies
{
  const QString Merge{"Merge"};
  const QString Append{"Append"};
  const QString Replace{"Replace"};
  operator QStringList() const { return {Merge, Append, Replace}; }
};
struct CommitPolicies
{
  const QString Default{"Default"};
  const QString Ordered{"Ordered"};
  const QString Priorized{"Priorized"};
  const QString Merged{"Merged"};
  operator QStringList() const { return {Default, Ordered, Priorized, Merged}; }
};
struct TickPolicies
{
  const QString Buffer{"Buffer-accurate"};
  const QString ScoreAccurate{"Score-accurate"};
  const QString Precise{"Precise"};
  operator QStringList() const { return {Buffer, ScoreAccurate, Precise}; }
};
class SCORE_PLUGIN_ENGINE_EXPORT Model : public score::SettingsDelegateModel
{
  W_OBJECT(Model)

  ClockFactory::ConcreteKey m_Clock;
  QString m_Scheduling;
  QString m_Ordering;
  QString m_Merging;
  QString m_Commit;
  QString m_Tick;
  int m_Rate{};
  bool m_Parallel{};
  bool m_ExecutionListening{};
  bool m_Logging{};
  bool m_Bench{};
  bool m_ScoreOrder{};
  bool m_ValueCompilation{};
  bool m_TransportValueCompilation{};

  const ClockFactoryList& m_clockFactories;

public:
  Model(QSettings& set, const score::ApplicationContext& ctx);

  const ClockFactoryList& clockFactories() const { return m_clockFactories; }

  std::unique_ptr<Clock> makeClock(const Execution::Context& ctx) const;
  time_function makeTimeFunction(const score::DocumentContext&) const;
  reverse_time_function makeReverseTimeFunction(const score::DocumentContext&) const;

  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, ClockFactory::ConcreteKey, Clock)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, QString, Scheduling)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, QString, Ordering)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, QString, Merging)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, QString, Commit)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, QString, Tick)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, int, Rate)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, bool, Parallel)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, bool, ExecutionListening)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, bool, Logging)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, bool, Bench)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, bool, ScoreOrder)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, bool, ValueCompilation)
  SCORE_SETTINGS_PARAMETER_HPP(SCORE_PLUGIN_ENGINE_EXPORT, bool, TransportValueCompilation)
};

SCORE_SETTINGS_PARAMETER(Model, Clock)
SCORE_SETTINGS_PARAMETER(Model, Scheduling)
SCORE_SETTINGS_PARAMETER(Model, Ordering)
SCORE_SETTINGS_PARAMETER(Model, Merging)
SCORE_SETTINGS_PARAMETER(Model, Commit)
SCORE_SETTINGS_PARAMETER(Model, Tick)
SCORE_SETTINGS_PARAMETER(Model, Rate)
SCORE_SETTINGS_PARAMETER(Model, Parallel)
SCORE_SETTINGS_PARAMETER(Model, ExecutionListening)
SCORE_SETTINGS_PARAMETER(Model, Logging)
SCORE_SETTINGS_PARAMETER(Model, Bench)
SCORE_SETTINGS_PARAMETER(Model, ScoreOrder)
SCORE_SETTINGS_PARAMETER(Model, ValueCompilation)
SCORE_SETTINGS_PARAMETER(Model, TransportValueCompilation)
}
}
