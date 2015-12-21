#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore_lib_process_export.h>
class Process;
class ProcessInspectorWidgetDelegate;
namespace iscore
{
struct DocumentContext;
}
class ISCORE_LIB_PROCESS_EXPORT ProcessInspectorWidgetDelegateFactory : public iscore::FactoryInterfaceBase
{
         ISCORE_FACTORY_DECL("ProcessInspectorWidgetDelegate")
    public:
        virtual ~ProcessInspectorWidgetDelegateFactory();
        virtual ProcessInspectorWidgetDelegate* make(
                 const Process&,
                 const iscore::DocumentContext& doc,
                 QWidget* parent) const = 0;
        virtual bool matches(const Process&) const = 0;
};
