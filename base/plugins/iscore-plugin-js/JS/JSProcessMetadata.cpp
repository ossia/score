#include "JSProcessMetadata.hpp"
const ProcessFactoryKey&JSProcessMetadata::factoryKey()
{
    static const ProcessFactoryKey name{"Javascript"};
    return name;
}

QString JSProcessMetadata::processObjectName()
{
    return "Javascript";
}

QString JSProcessMetadata::factoryPrettyName()
{
    return QObject::tr("Javascript");
}
