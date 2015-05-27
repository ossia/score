#include "AddressSettings.hpp"

#include <QDebug>

static const QMap<IOType, QString> map{
    {{IOType::In, "In"},
     {IOType::Out, "Out"},
     {IOType::InOut, "In/Out"}}
};
const QMap<IOType, QString>& IOTypeStringMap()
{
    return map;
}
