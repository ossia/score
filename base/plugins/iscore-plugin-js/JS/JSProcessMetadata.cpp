#include <QObject>


#include "JSProcessMetadata.hpp"
#include <Process/ProcessFactoryKey.hpp>

namespace JS
{
const ProcessFactoryKey& ProcessMetadata::factoryKey()
{
    static const ProcessFactoryKey name{"Javascript"};
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
