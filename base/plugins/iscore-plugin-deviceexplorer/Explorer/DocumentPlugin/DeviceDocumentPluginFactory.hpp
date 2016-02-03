#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
namespace Explorer
{
// TODO find addPluginModel and move them all out of the application plug-ins.
// TODO use the goddamn macros (ISCORE_CONCRETE_FACTORY_DECL & friends )everywhere
class DocumentPluginFactory final :
        public iscore::DocumentPluginFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("6e610e1f-9de2-4c36-90dd-0ef570002a21")

    public:
        iscore::DocumentPlugin* load(
                const VisitorVariant& var,
                iscore::DocumentContext& doc,
                QObject* parent) override;
};
}
