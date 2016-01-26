#include <QObject>


#include "JSProcessMetadata.hpp"
#include <Process/ProcessFactoryKey.hpp>

namespace JS
{
const ProcessFactoryKey& ProcessMetadata::concreteFactoryKey()
{
    static const ProcessFactoryKey name{"846a5de5-47f9-46c5-a898-013cb20951d0"};
    return name;
}

QString ProcessMetadata::processObjectName()
{
    return "Javascript";
}

QString ProcessMetadata::factoryPrettyName()
{
    return QObject::tr("Javascript");
}
}
