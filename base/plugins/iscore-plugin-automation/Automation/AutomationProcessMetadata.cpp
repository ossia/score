#include <qobject.h>
#include <qobjectdefs.h>

#include "AutomationProcessMetadata.hpp"
#include "Process/ProcessFactoryKey.hpp"

const ProcessFactoryKey& AutomationProcessMetadata::factoryKey()
{
    static const ProcessFactoryKey name{"Automation"};
    return name;
}

QString AutomationProcessMetadata::processObjectName()
{
    return "Automation";
}

QString AutomationProcessMetadata::factoryPrettyName()
{
    return QObject::tr("Automation");
}
