#pragma once
#include <QTime>
#include <iscore/tools/std/Optional.hpp>
#include <chrono>
#include "ZoomHelper.hpp"

//using namespace std::literals::chrono_literals;

class ZeroTime {};
class PositiveInfinity {};
template<typename T>
class TimeValue_T
{
    public:
        static constexpr TimeValue_T zero() {return ZeroTime{}; }
        static constexpr TimeValue_T infinite() {return PositiveInfinity{}; }
        static TimeValue_T fromMsecs(T msecs)
        {
            TimeValue_T time;
            time.m_impl = msecs;
            return time;
        }

        constexpr TimeValue_T() = default;
        constexpr TimeValue_T(PositiveInfinity) :
            m_impl {}
        { }

        constexpr TimeValue_T(ZeroTime) :
            m_impl {T(0)}
        { }

        TimeValue_T(QTime t):
            m_impl{T(t.msec() + 1000 * t.second() + 60000 * t.minute() + 3600000 * t.hour())}
        { }


        // These two overloads are here to please coverity...
        constexpr TimeValue_T(std::chrono::seconds&& dur) :
            m_impl {T(std::chrono::duration_cast<std::chrono::milliseconds> (dur).count())}
        { }
        constexpr TimeValue_T(std::chrono::milliseconds&& dur) :
            m_impl {T(dur.count())}
        { }

        template<typename Duration,
                 std::enable_if_t<std::is_class<Duration>::value>* = nullptr>
        constexpr TimeValue_T(Duration && dur) :
            m_impl {T(std::chrono::duration_cast<std::chrono::milliseconds> (dur).count()) }
        { }



        bool isInfinite() const
        {
            return !bool (m_impl);
        }

        bool isZero() const
        {
            return !isInfinite() && (msec() == 0);
        }

        T msec() const
        {
            if(!isInfinite())
                return *m_impl;

            return 0;
        }

        T sec() const
        {
            return double(*m_impl)/1000;
        }

        double toPixels(ZoomRatio ratio) const
        {
            return ratio > 0
                    ? *m_impl / ratio
                    : 0;
        }

        QTime toQTime() const
        {
            if (isInfinite())
                return QTime(23,59,59,999);
            else
                return QTime(0,0,0,0).addMSecs(static_cast<int>(*m_impl));
        }

        QString toString() const
        {
            QString t;
            auto qT = this->toQTime();

            if(qT.hour() != 0)
            {
                t += QString::number(qT.hour());
                t += " h ";
            }
            if(qT.minute() != 0)
            {
                t += QString::number(qT.minute());
                t += " min ";
            }

            t += QString::number(qT.second());
            t += " s ";
            t += QString::number(qT.msec());
            t += " ms";

            return t;
        }

        void addMSecs(T msecs)
        {
            if(m_impl)
            {
                *m_impl += msecs;
            }
        }

        void setMSecs(T msecs)
        {
            m_impl = msecs;
        }

        bool operator== (const TimeValue_T& other) const
        {
            return other.m_impl == m_impl;
        }

        bool operator!= (const TimeValue_T& other) const
        {
            return other.m_impl != m_impl;
        }

        bool operator> (const TimeValue_T& other) const
        {
            if(isInfinite() && other.isInfinite())
            {
                return false;
            }
            else if(isInfinite() && !other.isInfinite())
            {
                return true;
            }
            else if(!isInfinite() && other.isInfinite())
            {
                return false;
            }
            else
            {
                return msec() > other.msec();
            }
        }

        bool operator>= (const TimeValue_T& other) const
        {
            return *this > other || *this == other;
        }

        bool operator< (const TimeValue_T& other) const
        {
            if(isInfinite() && other.isInfinite())
            {
                return false;
            }
            else if(!isInfinite() && other.isInfinite())
            {
                return true;
            }
            else if(isInfinite() && !other.isInfinite())
            {
                return false;
            }
            else
            {
                return msec() < other.msec();
            }
        }

        bool operator<= (const TimeValue_T& other) const
        {
            return *this < other || *this == other;
        }

        TimeValue_T operator+ (const TimeValue_T& other) const
        {
            TimeValue_T res {PositiveInfinity{}};

            if(isInfinite() || other.isInfinite())
            {
                return res;
            }

            res.m_impl = *m_impl + *other.m_impl;
            return res;
        }

        TimeValue_T operator* (double other) const
        {
            TimeValue_T res {PositiveInfinity{}};

            if(isInfinite())
            {
                return res;
            }

            res.m_impl = *m_impl * other;
            return res;
        }

        double operator/ (const TimeValue_T& other) const
        {
            return double(*m_impl) / double(*other.m_impl);
        }

        TimeValue_T operator- (const TimeValue_T& other) const
        {
            TimeValue_T res {PositiveInfinity{}};

            if(isInfinite() || other.isInfinite())
            {
                return res;
            }

            res.m_impl = *m_impl - *other.m_impl;
            return res;
        }

        TimeValue_T operator- () const
        {
            TimeValue_T res{ZeroTime{}};
            TimeValue_T zero{ZeroTime{}};

            res.m_impl = *zero.m_impl - *m_impl;

            return res;
        }

        TimeValue_T operator+= (const TimeValue_T& other)
        {
            *this = *this + other;
            return *this;
        }

    private:
        optional<T> m_impl {T(0)}; // TODO std::isinf instead.
};

/*
template<>
class TimeValue_T<QTime>
{
    public:
        TimeValue_T<QTime>() = default;
        TimeValue_T<QTime>(int msec):
            TimeValue_T{}
        { m_impl.addMSecs(msec); }

        int msec() const
        { return m_impl.msec(); }

        void addMSecs(int msecs)
        { m_impl.addMSecs(msecs); }

        bool operator==(const TimeValue_T& other) const
        { return other.m_impl == m_impl; }

        bool operator!=(const TimeValue_T& other) const
        { return other.m_impl != m_impl; }

    private:
        QTime m_impl{0, 0};
};
*/
#include <QDebug>
using TimeValue = TimeValue_T<double>;
inline QDebug operator<< (QDebug d, const TimeValue& tv)
{
    if(!tv.isInfinite())
    {
        d << tv.msec() << "ms";
    }
    else
    {
        d << "infinite";
    }

    return d;
}
inline const TimeValue& max(const TimeValue& lhs, const TimeValue& rhs)
{
    if(lhs < rhs)
        return rhs;
    else
        return lhs;
}

#include <iscore/serialization/DataStreamVisitor.hpp>
template<>
struct TSerializer<DataStream, void, TimeValue>
{
        static void readFrom(
                DataStream::Serializer& s,
                const TimeValue& tv)
        {
            s.stream() << tv.isInfinite();

            if(!tv.isInfinite())
            {
                s.stream() << tv.msec();
            }
        }

        static void writeTo(
                DataStream::Deserializer& s,
                TimeValue& tv)
        {
            bool inf;
            s.stream() >> inf;

            if(!inf)
            {
                double msec;
                s.stream() >> msec;
                tv.setMSecs(msec);
            }
            else
            {
                tv = TimeValue {PositiveInfinity{}};
            }
        }
};

#include <iscore/serialization/JSONValueVisitor.hpp>


template<>
struct TSerializer<JSONValue, TimeValue>
{
    static void readFrom(
            JSONValue::Serializer& s,
            const TimeValue& tv)
    {
        if(tv.isInfinite())
        {
            s.val = "inf";
        }
        else
        {
            s.val = tv.msec();
        }
    }

    static void writeTo(
            JSONValue::Deserializer& s,
            TimeValue& tv)
    {
        if(s.val.toString() == "inf")
        {
            tv = TimeValue {PositiveInfinity{}};
        }
        else
        {
            tv.setMSecs(s.val.toDouble());
        }
    }
};


Q_DECLARE_METATYPE(TimeValue)
