#include <QMap>
#include <QObject>

#include <QString>

#include "ClipMode.hpp"

namespace Device
{

static const QMap<ClipMode, QString> clipmodemap{
    {{ClipMode::Free, QObject::tr("Free")},
     {ClipMode::Clip, QObject::tr("Clip")},
     {ClipMode::Fold, QObject::tr("Fold")},     
     {ClipMode::Wrap, QObject::tr("Wrap")},
    }
};
const QMap<ClipMode, QString> &ClipModeStringMap()
{
    return clipmodemap;
}
}
