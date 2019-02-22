// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ClipMode.hpp"

#include <QMap>
#include <QObject>
#include <QString>

namespace Device
{
const QMap<ossia::bounding_mode, QString> clipmodemap{{
    {ossia::bounding_mode::FREE, QStringLiteral("Free")},
    {ossia::bounding_mode::CLIP, QStringLiteral("Clip")},
    {ossia::bounding_mode::FOLD, QStringLiteral("Fold")},
    {ossia::bounding_mode::WRAP, QStringLiteral("Wrap")},
    {ossia::bounding_mode::LOW, QStringLiteral("Low")},
    {ossia::bounding_mode::HIGH, QStringLiteral("High")},
}};
const QMap<ossia::bounding_mode, QString> clipmodeprettymap{{
    {ossia::bounding_mode::FREE, QObject::tr("Free")},
    {ossia::bounding_mode::CLIP, QObject::tr("Clip")},
    {ossia::bounding_mode::FOLD, QObject::tr("Fold")},
    {ossia::bounding_mode::WRAP, QObject::tr("Wrap")},
    {ossia::bounding_mode::LOW, QObject::tr("Low")},
    {ossia::bounding_mode::HIGH, QObject::tr("High")},
}};
const QMap<ossia::bounding_mode, QString>& ClipModeStringMap()
{
  return clipmodemap;
}
const QMap<ossia::bounding_mode, QString>& ClipModePrettyStringMap()
{
  return clipmodeprettymap;
}
}
