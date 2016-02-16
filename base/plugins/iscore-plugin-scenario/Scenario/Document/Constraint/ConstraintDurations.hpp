#pragma once
#include <Process/TimeValue.hpp>
#include <QObject>
#include <chrono>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore_plugin_scenario_export.h>

class DataStream;
class JSONObject;

namespace Scenario
{
class ConstraintModel;

// A container class to separate management of the duration of a constraint.
class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintDurations final :
        public QObject
{
        // These dates are relative to the beginning of the constraint.
        Q_PROPERTY(TimeValue minDuration
                   READ minDuration
                   WRITE setMinDuration
                   NOTIFY minDurationChanged)
        Q_PROPERTY(TimeValue maxDuration
                   READ maxDuration
                   WRITE setMaxDuration
                   NOTIFY maxDurationChanged)
        Q_PROPERTY(double playPercentage
                   READ playPercentage
                   WRITE setPlayPercentage
                   NOTIFY playPercentageChanged)

        Q_PROPERTY(bool isRigid
                   READ isRigid
                   WRITE setRigid
                   NOTIFY rigidityChanged)
        Q_PROPERTY(bool isMinNul
                   READ isMinNul
                   WRITE setMinNull
                   NOTIFY minNullChanged)
        Q_PROPERTY(bool isMaxInfinite
                   READ isMaxInfinite
                   WRITE setMaxInfinite
                   NOTIFY maxInfiniteChanged)

        Q_PROPERTY(double executionSpeed
                   READ executionSpeed
                   WRITE setExecutionSpeed
                   NOTIFY executionSpeedChanged)

        ISCORE_SERIALIZE_FRIENDS(ConstraintDurations, DataStream)
        ISCORE_SERIALIZE_FRIENDS(ConstraintDurations, JSONObject)

        Q_OBJECT
    public:
        ConstraintDurations(ConstraintModel& model):
            m_model{model}
        {

        }

        ConstraintDurations& operator=(const ConstraintDurations& other);

        const TimeValue& defaultDuration() const
        { return m_defaultDuration; }

        TimeValue minDuration() const
        {
            if(m_isMinNull)
                return TimeValue::zero();
            return m_minDuration;
        }

        TimeValue maxDuration() const
        {
            if(m_isMaxInfinite)
                return PositiveInfinity{};
            return m_maxDuration;
        }

        double playPercentage() const
        { return m_playPercentage; }

        double executionSpeed() const
        {  return m_executionSpeed; }

        bool isRigid() const
        { return m_rigidity; }

        void checkConsistency();

        // Modification algorithms that keep everything consistent
        class Algorithms
        {
            public:
                static void setDurationInBounds(ConstraintModel& cstr, const TimeValue& time);
                static void changeAllDurations(ConstraintModel& cstr, const TimeValue& time);
        };

        bool isMinNul() const
        { return m_isMinNull; }

        bool isMaxInfinite() const
        { return m_isMaxInfinite; }

        void setDefaultDuration(const TimeValue& arg);
        void setMinDuration(const TimeValue& arg);
        void setMaxDuration(const TimeValue& arg);

        void setPlayPercentage(double arg);

        void setRigid(bool arg);

        void setMinNull(bool isMinNull);

        void setMaxInfinite(bool isMaxInfinite);

        void setExecutionSpeed(double executionSpeed)
        {
            if (m_executionSpeed == executionSpeed)
                return;

            m_executionSpeed = executionSpeed;
            emit executionSpeedChanged(executionSpeed);
        }

    signals:
        void defaultDurationChanged(const TimeValue& arg);
        void minDurationChanged(const TimeValue& arg);
        void maxDurationChanged(const TimeValue& arg);

        void playPercentageChanged(double arg);
        void rigidityChanged(bool arg);

        void minNullChanged(bool isMinNul);

        void maxInfiniteChanged(bool isMaxInfinite);

        void executionSpeedChanged(double executionSpeed);

    private:
        ConstraintModel& m_model;

        TimeValue m_defaultDuration{std::chrono::milliseconds{200}};
        TimeValue m_minDuration{m_defaultDuration};
        TimeValue m_maxDuration{m_defaultDuration};

        double m_playPercentage{}; // Between 0 and 1.
        double m_executionSpeed{1};
        bool m_rigidity{true};
        bool m_isMinNull{false};
        bool m_isMaxInfinite{false};
};
}
