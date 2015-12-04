#pragma once
#include <QObject>
#include <QStringList>
#include <iscore_lib_base_export.h>

namespace iscore
{

    // Used to declare which plug-ins require to
    // have their ApplicationPlugin loaded prior to this one.
    class ISCORE_LIB_BASE_EXPORT PluginRequirementslInterface_QtInterface
    {
        public:
            virtual ~PluginRequirementslInterface_QtInterface();

            virtual QStringList required() const { return {}; }
            virtual QStringList offered() const { return {}; }
    };
}


#define PluginRequirementsInterface_QtInterface_iid "org.ossia.i-score.plugins.PluginRequirementslInterface_QtInterface"

Q_DECLARE_INTERFACE(iscore::PluginRequirementslInterface_QtInterface, PluginRequirementsInterface_QtInterface_iid)
