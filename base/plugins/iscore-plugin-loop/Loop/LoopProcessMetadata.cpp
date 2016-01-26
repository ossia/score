#include <QObject>


#include "LoopProcessMetadata.hpp"
#include <Process/ProcessFactoryKey.hpp>

namespace Loop
{
const ProcessFactoryKey& ProcessMetadata::concreteFactoryKey()
{
    static const ProcessFactoryKey name{"995d41a8-0f10-4152-971d-e4c033579a02"};
    return name;
}

QString ProcessMetadata::processObjectName()
{
    return "Loop";
}

QString ProcessMetadata::factoryPrettyName()
{
    return QObject::tr("Loop");
}
}
