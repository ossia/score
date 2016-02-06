#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore_lib_process_export.h>
namespace Process { class ProcessModel; }
class ProcessInspectorWidgetDelegate;
namespace iscore
{
struct DocumentContext;
}
class ISCORE_LIB_PROCESS_EXPORT ProcessInspectorWidgetDelegateFactory :
        public iscore::GenericFactoryInterface<UuidKey<ProcessInspectorWidgetDelegate>>
{
         ISCORE_ABSTRACT_FACTORY_DECL(
                 ProcessInspectorWidgetDelegate,
                 "75a45c5e-24ab-4ebb-ba57-195254a6847f")
    public:
        virtual ~ProcessInspectorWidgetDelegateFactory();
        virtual ProcessInspectorWidgetDelegate* make(
                 const Process::ProcessModel&,
                 const iscore::DocumentContext& doc,
                 QWidget* parent) const = 0;
        virtual bool matches(const Process::ProcessModel&) const = 0;
};
