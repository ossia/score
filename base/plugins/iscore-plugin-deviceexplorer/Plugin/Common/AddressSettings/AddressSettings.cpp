#include "AddressSettings.hpp"

#include <QDebug>

static const QMap<IOType, QString> iotypemap{
    {{IOType::Invalid, QObject::tr("")},
     {IOType::In, QObject::tr("<-")},
     {IOType::Out, QObject::tr("->")},
     {IOType::InOut, QObject::tr("<->")}}
};
const QMap<IOType, QString>& IOTypeStringMap()
{
    return iotypemap;
}


static const QMap<ClipMode, QString> clipmodemap{
    {{ClipMode::Clip, QObject::tr("Clip")},
     {ClipMode::Fold, QObject::tr("Fold")},
     {ClipMode::Free, QObject::tr("Free")},
     {ClipMode::Wrap, QObject::tr("Wrap")},
    }
};
const QMap<ClipMode, QString> &ClipModeStringMap()
{
    return clipmodemap;
}
