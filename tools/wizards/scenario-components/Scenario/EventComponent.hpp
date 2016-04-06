#pragma once
#include <Scenario/Document/Event/EventModel.hpp>
#include <Network/Node.h>
#include <Audio/AudioStreamEngine/AudioDocumentPlugin.hpp>

namespace Audio
{
namespace AudioStreamEngine
{
class EventComponent final :
        public iscore::Component
{

    public:
        using system_t = Audio::AudioStreamEngine::DocumentPlugin;

        EventComponent(
                const Id<Component>& id,
                Scenario::EventModel& event,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);

        const Key& key() const override;

        ~EventComponent();

    private:
};
}
}
