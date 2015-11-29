#include <qmap.h>
#include <qobject.h>
#include <qobjectdefs.h>
#include <qstring.h>

#include "ClipMode.hpp"

using namespace iscore;

static const QMap<ClipMode, QString> clipmodemap{
    {{ClipMode::Clip, QObject::tr("Clip")},
     {ClipMode::Fold, QObject::tr("Fold")},
     {ClipMode::Free, QObject::tr("Free")},
     {ClipMode::Wrap, QObject::tr("Wrap")},
    }
};
const QMap<ClipMode, QString> &iscore::ClipModeStringMap()
{
    return clipmodemap;
}
