#pragma once
#include <ProcessInterface/TimeValue.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

class ConstraintModel;
// A container class to separate management of the duration of a constraint.
class ConstraintDurations : public QObject
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
        Q_PROPERTY(TimeValue playDuration
                   READ playDuration
                   WRITE setPlayDuration
                   NOTIFY playDurationChanged)

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
        const TimeValue& playDuration() const
        { return m_playDuration; }

        bool isRigid() const
        { return m_rigidity; }

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

        void playDurationChanged(const TimeValue& arg);
        void rigidityChanged(bool arg);

    public slots:
        void setDefaultDuration(const TimeValue& arg);
        void setMinDuration(const TimeValue& arg);
        void setMaxDuration(const TimeValue& arg);

        void setPlayDuration(const TimeValue& arg);

        // TODO make a class that manages all the durations + rigidity in a coherent manner
        void setRigid(bool arg);

    private:
        ConstraintModel& m_model;

        TimeValue m_defaultDuration{std::chrono::milliseconds{200}};
        TimeValue m_minDuration{m_defaultDuration};
        TimeValue m_maxDuration{m_defaultDuration};

        TimeValue m_playDuration; // TODO should be a play percentage instead.
        bool m_rigidity{true};
};
