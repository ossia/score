#include "LoopProcessMetadata.hpp"
const ProcessFactoryKey&LoopProcessMetadata::factoryKey()
{
    static const ProcessFactoryKey name{"Loop"};
    return name;
}

QString LoopProcessMetadata::processObjectName()
{
    return "Loop";
}

QString LoopProcessMetadata::factoryPrettyName()
{
    return QObject::tr("Loop");
}
