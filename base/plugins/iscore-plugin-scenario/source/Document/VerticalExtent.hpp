#pragma once
#include <QPointF>
struct VerticalExtent
{
        VerticalExtent() = default;
        VerticalExtent(const VerticalExtent&) = default;
        VerticalExtent(VerticalExtent&&) = default;
        VerticalExtent(const QPointF& other): point{other} {}
        VerticalExtent(QPointF&& other): point{std::move(other)} {}
        VerticalExtent& operator =(const VerticalExtent& other) { point = other.point; return *this; }
        VerticalExtent& operator =(VerticalExtent&& other) { point = std::move(other.point); return *this; }
        VerticalExtent& operator =(const QPointF& other) { point = other; return *this;  }
        VerticalExtent& operator =(QPointF&& other) { point = std::move(other); return *this; }

        constexpr double top() const    { return point.x(); }
        constexpr double bottom() const { return point.y(); }

        operator QPointF&() { return point; }
        operator const QPointF&() const { return point; }
        QPointF point;
};

#include <QDebug>
inline QDebug operator<< (QDebug d, const VerticalExtent& ve)
{
    d << ve.point;
    return d;
}
