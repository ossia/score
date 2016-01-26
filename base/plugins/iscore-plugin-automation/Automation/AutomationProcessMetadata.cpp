#include <QObject>


#include "AutomationProcessMetadata.hpp"
#include <Process/ProcessFactoryKey.hpp>

namespace Automation
{
const ProcessFactoryKey& ProcessMetadata::concreteFactoryKey()
{
    static const ProcessFactoryKey name{"d2a67bd8-5d3f-404e-b6e9-e350cf2a833f"};
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
