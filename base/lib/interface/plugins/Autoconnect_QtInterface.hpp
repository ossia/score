#pragma once
#include <interface/autoconnect/Autoconnect.hpp>

namespace iscore
{
    class Autoconnect_QtInterface
    {
        public:
            virtual ~Autoconnect_QtInterface() = default;
            // List the Processes offered by the plugin.

            virtual QList<iscore::Autoconnect> autoconnect_list() const = 0;
    };
}

#define Autoconnect_QtInterface_iid "org.ossia.i-score.plugins.Autoconnect_QtInterface"

Q_DECLARE_INTERFACE(iscore::Autoconnect_QtInterface, Autoconnect_QtInterface_iid)
