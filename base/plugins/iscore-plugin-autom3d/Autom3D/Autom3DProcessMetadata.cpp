#include <QObject>


#include "Autom3DProcessMetadata.hpp"
#include <Process/ProcessFactoryKey.hpp>

namespace Autom3D
{
const ProcessFactoryKey& ProcessMetadata::factoryKey()
{
    static const ProcessFactoryKey name{"Autom3D"};
    return name;
}

QString ProcessMetadata::processObjectName()
{
    return "Autom3D";
}

QString ProcessMetadata::factoryPrettyName()
{
    return QObject::tr("Autom3D");
}
}
