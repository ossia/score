#pragma once
#include <iscore/plugins/customfactory/FactoryInterface.hpp>
#include <iscore_lib_process_export.h>
namespace Process {
class ProcessModel;
class StateProcess;
class InspectorWidgetDelegate;
class StateProcessInspectorWidgetDelegate;
}
namespace iscore
{
struct DocumentContext;
}

namespace Process
{
class ISCORE_LIB_PROCESS_EXPORT InspectorWidgetDelegateFactory :
        public iscore::AbstractFactory<InspectorWidgetDelegateFactory>
{
         ISCORE_ABSTRACT_FACTORY_DECL(
                 InspectorWidgetDelegateFactory,
                 "75a45c5e-24ab-4ebb-ba57-195254a6847f")
    public:
        virtual ~InspectorWidgetDelegateFactory();
        virtual Process::InspectorWidgetDelegate* make(
                 const Process::ProcessModel&,
                 const iscore::DocumentContext& doc,
                 QWidget* parent) const = 0;
        virtual bool matches(const Process::ProcessModel&) const = 0;

         bool matches(
                 const Process::ProcessModel& proc,
                 const iscore::DocumentContext& doc,
                 QWidget* parent) const
         {
             return matches(proc);
         }
};


class ISCORE_LIB_PROCESS_EXPORT StateProcessInspectorWidgetDelegateFactory :
        public iscore::AbstractFactory<StateProcessInspectorWidgetDelegateFactory>
{
         ISCORE_ABSTRACT_FACTORY_DECL(
                 StateProcessInspectorWidgetDelegateFactory,
                 "707f7d38-7897-4b8f-81eb-737976b05ea6")
    public:
        virtual ~StateProcessInspectorWidgetDelegateFactory();
        virtual Process::StateProcessInspectorWidgetDelegate* make(
                 const Process::StateProcess&,
                 const iscore::DocumentContext& doc,
                 QWidget* parent) const = 0;
        virtual bool matches(const Process::StateProcess&) const = 0;

         bool matches(
                 const Process::StateProcess& proc,
                 const iscore::DocumentContext& doc,
                 QWidget* parent) const
         {
             return matches(proc);
         }
};
}
