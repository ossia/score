#include "IOType.hpp"
#include <QObject>
using namespace iscore;

static const QMap<IOType, QString> iotypemap{
    {
    //    {IOType::Invalid, QObject::tr("")},
        {IOType::In, QObject::tr("<-")},
        {IOType::Out, QObject::tr("->")},
        {IOType::InOut, QObject::tr("<->")}
    }
};

const QMap<IOType, QString>& iscore::IOTypeStringMap()
{
    return iotypemap;
}
