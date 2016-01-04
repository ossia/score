#pragma once
#include <QObject>
#include <Process/Process.hpp>
#include <memory>
#include <iscore/component/Component.hpp>
#include <iscore/component/ComponentFactory.hpp>
#include <iscore_plugin_ossia_export.h>
namespace OSSIA
{
    class Scenario;
    class TimeProcess;
}

namespace RecreateOnPlay
{
struct Context;
class DocumentPlugin;
class ConstraintElement;

class ISCORE_PLUGIN_OSSIA_EXPORT ProcessComponent :
        public iscore::Component
{
        ISCORE_METADATA(RecreateOnPlay::ProcessComponent)
    public:
        ProcessComponent(
                ConstraintElement& cst,
                Process::ProcessModel& proc,
                const Id<iscore::Component>& id,
                const QString& name,
                QObject* parent);

        virtual ~ProcessComponent();

        virtual std::shared_ptr<OSSIA::TimeProcess> OSSIAProcess() const = 0;
        virtual Process::ProcessModel& iscoreProcess() const = 0;
        virtual void stop()
        {
            iscoreProcess().stopExecution();
        }

    protected:
        ConstraintElement& m_parent_constraint;
};

class ISCORE_PLUGIN_OSSIA_EXPORT ProcessComponentFactory :
        public iscore::GenericComponentFactory<
            Process::ProcessModel,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::ProcessComponent>
{
    public:
        virtual ~ProcessComponentFactory();
        virtual ProcessComponent* make(
                  ConstraintElement& cst,
                  Process::ProcessModel& proc,
                  const Context& ctx,
                  const Id<iscore::Component>& id,
                  QObject* parent) const = 0;
};

using ProcessComponentFactoryList =
    iscore::GenericComponentFactoryList<
            Process::ProcessModel,
            RecreateOnPlay::DocumentPlugin,
            RecreateOnPlay::ProcessComponent,
            RecreateOnPlay::ProcessComponentFactory>;



}

