#include <QMap>
#include <QObject>

#include <QString>

#include "ClipMode.hpp"

namespace Device
{
const QMap<ClipMode, QString> clipmodemap{
    {{ClipMode::Free, QObject::tr("Free")},
     {ClipMode::Clip, QObject::tr("Clip")},
     {ClipMode::Fold, QObject::tr("Fold")},
     {ClipMode::Wrap, QObject::tr("Wrap")},
     {ClipMode::Low, QObject::tr("Low")},
     {ClipMode::High, QObject::tr("High")},
    }
};
const QMap<ClipMode, QString> &ClipModeStringMap()
{
    return clipmodemap;
}
}
