#pragma once
#include <Audio/AudioStreamEngine/Scenario/ProcessComponent.hpp>
#include <Audio/AudioStreamEngine/AudioDocumentPlugin.hpp>
#include <Scenario/Document/Components/ConstraintComponent.hpp>

namespace Audio
{
namespace AudioStreamEngine
{
class Constraint final :
        public iscore::Component
{
    public:
        using system_t = Audio::AudioStreamEngine::DocumentPlugin;
        using process_component_t = Audio::AudioStreamEngine::ProcessComponent;
        using process_component_factory_t = Audio::AudioStreamEngine::ProcessComponentFactory;
        using process_component_factory_list_t = Audio::AudioStreamEngine::ProcessComponentFactoryList;

        using parent_t = ::ConstraintComponentHierarchyManager<
            Constraint,
            system_t,
            process_component_t,
            process_component_factory_list_t
        >;

        const Key& key() const override;

        Constraint(
                const Id<Component>& id,
                Scenario::ConstraintModel& constraint,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);
        ~Constraint();

        ProcessComponent* make_processComponent(
                const Id<Component> & id,
                ProcessComponentFactory& factory,
                Process::ProcessModel &process,
                const DocumentPlugin &system);

        void removing(const Process::ProcessModel& cst, const ProcessComponent& comp);

    private:
        parent_t m_baseComponent;
};



}
}
