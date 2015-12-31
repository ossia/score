#include <QObject>


#include "AutomationProcessMetadata.hpp"
#include <Process/ProcessFactoryKey.hpp>

namespace Automation
{
const ProcessFactoryKey& ProcessMetadata::factoryKey()
{
    static const ProcessFactoryKey name{"Automation"};
    return name;
}

QString ProcessMetadata::processObjectName()
{
    return "Automation";
}

QString ProcessMetadata::factoryPrettyName()
{
    return QObject::tr("Automation");
}
}
