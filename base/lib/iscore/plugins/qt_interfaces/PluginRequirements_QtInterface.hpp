#pragma once
#include <QObject>

namespace iscore
{
    // If a plug-in requires or offer specific capabilities
    // They can be queried here.
    // The plug-ins will always be loaded in this order.

    class PluginRequirementslInterface_QtInterface
    {
        public:
            enum class LoadingOrder { BeforeDocument, AfterDocument };
            virtual ~PluginControlInterface_QtInterface() = default;

            virtual QStringList required() = 0;
            virtual QStringList offered() = 0;
            virtual LoadingOrder order() = 0;
    };
}


#define PluginControlInterface_QtInterface_iid "org.ossia.i-score.plugins.PluginRequirementslInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::PluginRequirementslInterface_QtInterface, PluginRequirementslInterface_QtInterface_iid)
