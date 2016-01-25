#include <QObject>


#include "LoopProcessMetadata.hpp"
#include <Process/ProcessFactoryKey.hpp>

namespace Loop
{
const ProcessFactoryKey& ProcessMetadata::abstractFactoryKey()
{
    static const ProcessFactoryKey name{"Loop"};
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
