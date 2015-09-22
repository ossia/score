#pragma once
#include <QObject>

namespace iscore
{

    // Used to declare which plug-ins require to
    // have their Control loaded prior to this one.
    class PluginRequirementslInterface_QtInterface
    {
        public:
            virtual ~PluginRequirementslInterface_QtInterface() = default;

            virtual QStringList required() const { return {}; }
            virtual QStringList offered() const { return {}; }
    };
}


#define PluginRequirementsInterface_QtInterface_iid "org.ossia.i-score.plugins.PluginRequirementslInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::PluginRequirementslInterface_QtInterface, PluginRequirementsInterface_QtInterface_iid)
