#include <QMap>
#include <QObject>

#include <QString>

#include "IOType.hpp"

namespace Device
{

static const QMap<IOType, QString> iotypemap{
    {
    //    {IOType::Invalid, QObject::tr("")},
        {IOType::In, QObject::tr("<-")},
        {IOType::Out, QObject::tr("->")},
        {IOType::InOut, QObject::tr("<->")}
    }
};

const QMap<IOType, QString>& Device::IOTypeStringMap()
{
    return iotypemap;
}
}
