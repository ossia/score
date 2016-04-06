#pragma once
#include <Scenario/Document/State/StateModel.hpp>
#include <Network/Node.h>
#include <Audio/AudioStreamEngine/AudioDocumentPlugin.hpp>

namespace Audio
{
namespace AudioStreamEngine
{
class StateComponent final :
        public iscore::Component
{
    public:
        using system_t = Audio::AudioStreamEngine::DocumentPlugin;

        StateComponent(
                const Id<iscore::Component>& id,
                Scenario::StateModel& state,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);

        const Key& key() const override;

        ~StateComponent();

    private:

};
}
}
