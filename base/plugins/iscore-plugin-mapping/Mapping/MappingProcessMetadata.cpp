#include <QObject>


#include "MappingProcessMetadata.hpp"
#include <Process/ProcessFactoryKey.hpp>

const ProcessFactoryKey&MappingProcessMetadata::factoryKey()
{
    static const ProcessFactoryKey name{"Mapping"};
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
