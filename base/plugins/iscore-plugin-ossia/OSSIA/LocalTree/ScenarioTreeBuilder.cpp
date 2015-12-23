#include "ScenarioTreeBuilder.hpp"

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>

#include <iscore/document/DocumentContext.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentDelegatePluginModel.hpp>
// Structure qui crée des objets qui se créent récursivement à l'insertion d'enfants
// dans le scénario.
// Doivent être créés après insertion, et supprimés avant suppression.
// Faire init de l'existant, et connection.
// Création initiale se fait par document plugin, ne sont pas sauvés / rechargés.

// Pattern général de création, puis delegate.

class ProcessComponentFactory;
using ProcessComponentFactoryKey =  StringKey<ProcessComponentFactory>;
class ProcessComponentFactory : public GenericFactoryInterface<ProcessComponentFactoryKey>
{
        ISCORE_FACTORY_DECL("ProcessComponentFactory")
        public:
        using factory_key_type = ProcessComponentFactoryKey;

};

class ProcessComponentFactoryList : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(ProcessComponentFactory)
    public:
        ProcessComponentFactory* make(
                Process&,
                iscore::DocumentPluginModel&,
                const iscore::DocumentContext&) const
        {
            return nullptr;
        }
};

struct ScenarioTreeBuilder
{
         iscore::DocumentPluginModel& m_doc;

        void build(
                Scenario::ScenarioModel& scenario,
                const iscore::DocumentContext& ctx)
        {

            for(auto& constraint : scenario.constraints)
            {

            }
        }

        void build(
                ConstraintModel& constraint,
                const iscore::DocumentContext& ctx)
        {
            auto& list = ctx.app.components.factory<ProcessComponentFactoryList>();
            auto processFun = [&] (Process& process) {
                if(auto component = list.make(process, m_doc, ctx))
                {
                    //process.components.add(component);
                }
            };

            for(auto& process : constraint.processes)
            {
                processFun(process);
            }

            constraint.processes.mutable_added.connect(processFun);

        }
};
