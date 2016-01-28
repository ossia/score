#pragma once
#include <QObject>
#include <vector>
#include <iscore_lib_base_export.h>

namespace iscore
{
    class PanelFactory;

    class ISCORE_LIB_BASE_EXPORT PanelFactory_QtInterface
    {
        public:
            virtual ~PanelFactory_QtInterface();

            virtual std::vector<PanelFactory*> panels() = 0;
    };
}

#define PanelFactory_QtInterface_iid "org.ossia.i-score.plugins.PanelFactory_QtInterface"

Q_DECLARE_INTERFACE(iscore::PanelFactory_QtInterface, PanelFactory_QtInterface_iid)
