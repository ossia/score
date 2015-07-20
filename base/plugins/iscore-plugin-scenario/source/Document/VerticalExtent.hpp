#pragma once
#include <QPointF>
struct VerticalExtent
{
        constexpr VerticalExtent() = default;
        constexpr VerticalExtent(const VerticalExtent&) = default;
        constexpr VerticalExtent(VerticalExtent&&) = default;
        constexpr VerticalExtent(const QPointF& other): point{other} {}
        constexpr VerticalExtent(QPointF&& other): point{std::move(other)} {}
        VerticalExtent& operator =(const VerticalExtent& other) { point = other.point; return *this; }
        VerticalExtent& operator =(VerticalExtent&& other) { point = std::move(other.point); return *this; }
        VerticalExtent& operator =(const QPointF& other) { point = other; return *this;  }
        VerticalExtent& operator =(QPointF&& other) { point = std::move(other); return *this; }

        constexpr double top() const    { return point.x(); }
        constexpr double bottom() const { return point.y(); }

        constexpr operator QPointF&() { return point; }
        constexpr operator const QPointF&() const { return point; }
        QPointF point;
};

#include <QDebug>
inline QDebug operator<< (QDebug d, const VerticalExtent& ve)
{
    d << ve.point;
    return d;
}
