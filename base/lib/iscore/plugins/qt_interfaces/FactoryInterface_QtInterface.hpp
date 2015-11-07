#pragma once
#include <QObject>
#include <vector>

namespace iscore
{
    class FactoryInterface;
    // Reimplement in plug-in if the plug-in offers an IMPLEMENTATION of an abstract type offered in another plug-in.
    // Example : the Scenario plug-in provides inspector widget factories implementations for Interval and event
    class FactoryInterface_QtInterface
    {
        public:
            virtual ~FactoryInterface_QtInterface() = default;
            virtual std::vector<FactoryInterface*> factories(const QString& matchingName) = 0;
    };
}


#define FactoryInterface_QtInterface_iid "org.ossia.i-score.plugins.FactoryInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::FactoryInterface_QtInterface, FactoryInterface_QtInterface_iid)

