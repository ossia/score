#pragma once
#include <score/serialization/VisitorInterface.hpp>

#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QSizeF>
#include <QString>

template <>
struct is_custom_serialized<QColor> : std::true_type
{
};
template <>
struct is_custom_serialized<QPoint> : std::true_type
{
};
template <>
struct is_custom_serialized<QPointF> : std::true_type
{
};
template <>
struct is_custom_serialized<QSize> : std::true_type
{
};
template <>
struct is_custom_serialized<QSizeF> : std::true_type
{
};
template <>
struct is_custom_serialized<QRect> : std::true_type
{
};
template <>
struct is_custom_serialized<QRectF> : std::true_type
{
};
