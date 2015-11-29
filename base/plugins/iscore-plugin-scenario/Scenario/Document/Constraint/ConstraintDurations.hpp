#pragma once
#include <Process/TimeValue.hpp>
#include <qobject.h>
#include <chrono>

class ConstraintModel;
class DataStream;
class JSONObject;

// A container class to separate management of the duration of a constraint.
class ConstraintDurations final : public QObject
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
        const TimeValue& minDuration() const
        { return m_minDuration; }
        const TimeValue& maxDuration() const
        { return m_maxDuration; }
        double playPercentage() const
        { return m_playPercentage; }

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

    signals:
        void defaultDurationChanged(const TimeValue& arg);
        void minDurationChanged(const TimeValue& arg);
        void maxDurationChanged(const TimeValue& arg);

        void playPercentageChanged(double arg);
        void rigidityChanged(bool arg);

    public slots:
        void setDefaultDuration(const TimeValue& arg);
        void setMinDuration(const TimeValue& arg);
        void setMaxDuration(const TimeValue& arg);

        void setPlayPercentage(double arg);

        // TODO make a class that manages all the durations + rigidity in a coherent manner
        void setRigid(bool arg);

    private:
        ConstraintModel& m_model;

        TimeValue m_defaultDuration{std::chrono::milliseconds{200}};
        TimeValue m_minDuration{m_defaultDuration};
        TimeValue m_maxDuration{m_defaultDuration};

        double m_playPercentage{}; // Between 0 and 1.
        bool m_rigidity{true};
};
