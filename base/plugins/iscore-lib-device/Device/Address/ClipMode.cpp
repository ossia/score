#include <QMap>
#include <QObject>

#include <QString>

#include "ClipMode.hpp"

namespace Device
{

static const QMap<ClipMode, QString> clipmodemap{
    {{ClipMode::Clip, QObject::tr("Clip")},
     {ClipMode::Fold, QObject::tr("Fold")},
     {ClipMode::Free, QObject::tr("Free")},
     {ClipMode::Wrap, QObject::tr("Wrap")},
    }
};
const QMap<ClipMode, QString> &Device::ClipModeStringMap()
{
    return clipmodemap;
}
}
