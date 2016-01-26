#pragma once
#include <Process/ProcessFactoryKey.hpp>

struct ScenarioProcessMetadata
{
        static const ProcessFactoryKey& concreteFactoryKey()
        {
            static const ProcessFactoryKey name{"de035912-5b03-49a8-bc4d-b2cba68e21d9"};
            return name;
        }

        static QString processObjectName()
        {
            return "Scenario";
        }

        static QString factorydescription()
        {
            return QObject::tr("Scenario");
        }
};
