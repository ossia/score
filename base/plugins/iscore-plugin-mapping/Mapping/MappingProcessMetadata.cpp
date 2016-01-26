#include <QObject>


#include "MappingProcessMetadata.hpp"
#include <Process/ProcessFactoryKey.hpp>

namespace Mapping
{
const ProcessFactoryKey&MappingProcessMetadata::concreteFactoryKey()
{
    static const ProcessFactoryKey name{"12a5d9b8-823e-4303-99f8-34db37c448b4"};
    return name;
}

QString MappingProcessMetadata::processObjectName()
{
    return "Mapping";
}

QString MappingProcessMetadata::factoryPrettyName()
{
    return QObject::tr("Mapping");
}
}
