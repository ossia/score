#pragma once
#include <Engine/Executor/ClockManager/ClockManagerFactory.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <ossia/editor/scenario/time_value.hpp>
#include <Process/TimeValue.hpp>
#include <score_plugin_engine_export.h>
namespace Engine
{
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
    operator QStringList() const {
      return { StaticFixed, StaticBFS, StaticTC, Dynamic };
    }
};
struct OrderingPolicies
{
    const QString CreationOrder{"Creation order"};
    const QString XY{"XY"};
    const QString YX{"YX"};
    const QString Temporal{"Temporal"};
    operator QStringList() const {
      return { CreationOrder, XY, YX, Temporal };
    }
};
struct MergingPolicies
{
    const QString Merge{"Merge"};
    const QString Append{"Append"};
    const QString Replace{"Replace"};
    operator QStringList() const {
      return { Merge, Append, Replace };
    }
};
struct CommitPolicies
{
    const QString Default{"Default"};
    const QString Ordered{"Ordered"};
    const QString Priorized{"Priorized"};
    const QString Merged{"Merged"};
    operator QStringList() const {
      return { Default, Ordered, Priorized, Merged  };
    }
};
struct TickPolicies
{
    const QString Buffer{"Buffer-accurate"};
    const QString ScoreAccurate{"Score-accurate"};
    const QString Precise{"Precise"};
    operator QStringList() const {
      return { Buffer, ScoreAccurate, Precise };
    }
};
class SCORE_PLUGIN_ENGINE_EXPORT Model : public score::SettingsDelegateModel
{
    Q_OBJECT
    Q_PROPERTY(ClockManagerFactory::ConcreteKey clock READ getClock WRITE setClock NOTIFY ClockChanged)
    Q_PROPERTY(QString scheduling READ getScheduling WRITE setScheduling NOTIFY SchedulingChanged)
    Q_PROPERTY(QString ordering READ getOrdering WRITE setOrdering NOTIFY OrderingChanged)
    Q_PROPERTY(QString merging READ getMerging WRITE setMerging NOTIFY MergingChanged)
    Q_PROPERTY(QString commit READ getCommit WRITE setCommit NOTIFY CommitChanged)
    Q_PROPERTY(QString tick READ getTick WRITE setTick NOTIFY TickChanged)
    Q_PROPERTY(int rate READ getRate WRITE setRate NOTIFY RateChanged)
    Q_PROPERTY(bool parallel READ getParallel WRITE setParallel NOTIFY ParallelChanged)
    Q_PROPERTY(bool executionListening READ getExecutionListening WRITE setExecutionListening NOTIFY ExecutionListeningChanged)
    Q_PROPERTY(bool logging READ getLogging WRITE setLogging NOTIFY LoggingChanged)
    Q_PROPERTY(bool scoreOrder READ getScoreOrder WRITE setScoreOrder NOTIFY ScoreOrderChanged)

    ClockManagerFactory::ConcreteKey m_Clock;
    QString m_Scheduling;
    QString m_Ordering;
    QString m_Merging;
    QString m_Commit;
    QString m_Tick;
    int m_Rate{};
    bool m_Parallel{};
    bool m_ExecutionListening{};
    bool m_Logging{};
    bool m_ScoreOrder{};

    const ClockManagerFactoryList& m_clockFactories;

  public:
    Model(QSettings& set, const score::ApplicationContext& ctx);

    const ClockManagerFactoryList& clockFactories() const
    {
      return m_clockFactories;
    }

    std::unique_ptr<ClockManager>
    makeClock(const Engine::Execution::Context& ctx) const;
    time_function
    makeTimeFunction(const score::DocumentContext&) const;
    reverse_time_function
    makeReverseTimeFunction(const score::DocumentContext&) const;

    SCORE_SETTINGS_PARAMETER_HPP(ClockManagerFactory::ConcreteKey, Clock)
    SCORE_SETTINGS_PARAMETER_HPP(QString, Scheduling)
    SCORE_SETTINGS_PARAMETER_HPP(QString, Ordering)
    SCORE_SETTINGS_PARAMETER_HPP(QString, Merging)
    SCORE_SETTINGS_PARAMETER_HPP(QString, Commit)
    SCORE_SETTINGS_PARAMETER_HPP(QString, Tick)
    SCORE_SETTINGS_PARAMETER_HPP(int, Rate)
    SCORE_SETTINGS_PARAMETER_HPP(bool, Parallel)
    SCORE_SETTINGS_PARAMETER_HPP(bool, ExecutionListening)
    SCORE_SETTINGS_PARAMETER_HPP(bool, Logging)
    SCORE_SETTINGS_PARAMETER_HPP(bool, ScoreOrder)
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
SCORE_SETTINGS_PARAMETER(Model, ScoreOrder)
}
}
}
