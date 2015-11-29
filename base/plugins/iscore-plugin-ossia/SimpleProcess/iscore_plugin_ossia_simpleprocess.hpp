#pragma once
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <qobject.h>
#include <vector>

#include "core/application/ApplicationContext.hpp"
#include "iscore/plugins/customfactory/FactoryInterface.hpp"

class iscore_plugin_ossia_simpleprocess:
        public QObject,
        public iscore::FactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::FactoryInterface_QtInterface
                )

    public:
        iscore_plugin_ossia_simpleprocess();
        virtual ~iscore_plugin_ossia_simpleprocess();

        // Process & inspector
        std::vector<iscore::FactoryInterfaceBase*> factories(
                const iscore::ApplicationContext&,
                const iscore::FactoryBaseKey& factoryName) const override;
};
