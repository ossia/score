#pragma once
#include <QPointF>
#include <iscore/Settings.hpp>

struct VerticalExtent
{
        Q_DECL_CONSTEXPR VerticalExtent() = default;
        Q_DECL_CONSTEXPR VerticalExtent(const VerticalExtent&) = default;
        Q_DECL_CONSTEXPR VerticalExtent(VerticalExtent&&) = default;
        Q_DECL_CONSTEXPR VerticalExtent(const QPointF& other): point{other} {}
        Q_DECL_CONSTEXPR VerticalExtent(QPointF&& other): point{std::move(other)} {}
        VerticalExtent& operator =(const VerticalExtent& other) { point = other.point; return *this; }
        VerticalExtent& operator =(VerticalExtent&& other) { point = std::move(other.point); return *this; }
        VerticalExtent& operator =(const QPointF& other) { point = other; return *this;  }
        VerticalExtent& operator =(QPointF&& other) { point = std::move(other); return *this; }

        Q_DECL_CONSTEXPR double top() const    { return point.x(); }
        Q_DECL_CONSTEXPR double bottom() const { return point.y(); }

        operator QPointF&() { return point; }
        Q_DECL_CONSTEXPR operator const QPointF&() const { return point; }
        QPointF point;
};

#include <QDebug>
inline QDebug operator<< (QDebug d, const VerticalExtent& ve)
{
    d << ve.point;
    return d;
}
