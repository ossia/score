#pragma once
#include <Audio/AudioStreamEngine/Scenario/ConstraintComponent.hpp>
#include <Audio/AudioStreamEngine/Scenario/EventComponent.hpp>
#include <Audio/AudioStreamEngine/Scenario/TimeNodeComponent.hpp>
#include <Audio/AudioStreamEngine/Scenario/StateComponent.hpp>
#include <Scenario/Document/Components/ScenarioComponent.hpp>

namespace Audio
{
namespace AudioStreamEngine
{
class ScenarioComponent final : public ProcessComponent
{
       COMPONENT_METADATA(Audio::AudioStreamEngine::ScenarioComponent)

        using system_t = Audio::AudioStreamEngine::DocumentPlugin;
        using hierarchy_t =
           ScenarioComponentHierarchyManager<
               ScenarioComponent,
               system_t,
               ConstraintComponent,
               EventComponent,
               TimeNodeComponent,
               StateComponent
        >;

    public:
       ScenarioComponent(
               const Id<Component>& id,
               Scenario::ScenarioModel& scenario,
               const system_t& doc,
               const iscore::DocumentContext& ctx,
               QObject* parent_obj);

       template<typename Component_T, typename Element>
       Component_T* make(
               const Id<Component>& id,
               Element& elt,
               const system_t& doc,
               const iscore::DocumentContext& ctx,
               QObject* parent);

        void removing(
                const Scenario::ConstraintModel& elt,
                const ConstraintComponent& comp);

        void removing(
                const Scenario::EventModel& elt,
                const EventComponent& comp);

        void removing(
                const Scenario::TimeNodeModel& elt,
                const TimeNodeComponent& comp);

        void removing(
                const Scenario::StateModel& elt,
                const StateComponent& comp);

    private:
        hierarchy_t m_hm;

};

}
}
