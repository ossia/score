#pragma once
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Network/Node.h>
#include <Audio/AudioStreamEngine/AudioDocumentPlugin.hpp>


namespace Audio
{
namespace AudioStreamEngine
{
class TimeNodeComponent final :
        public iscore::Component
{
    public:
        using system_t = Audio::AudioStreamEngine::DocumentPlugin;

        TimeNodeComponent(
                const Id<iscore::Component>& id,
                Scenario::TimeNodeModel& timeNode,
                const system_t& doc,
                const iscore::DocumentContext& ctx,
                QObject* parent_comp);

        const Key& key() const override;

        ~TimeNodeComponent();

    private:
};
}
}
