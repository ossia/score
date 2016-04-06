#pragma once
#include <Audio/AudioStreamEngine/Scenario/ProcessComponent.hpp>

namespace Audio
{
namespace AudioStreamEngine
{

class ScenarioComponentFactory final :
        public ProcessComponentFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("${JS: guid()}")
        bool matches(
                Process::ProcessModel& p,
                const Audio::AudioStreamEngine::DocumentPlugin&,
                const iscore::DocumentContext&) const override;

        ProcessComponent* make(
                const Id<iscore::Component>& id,
                Process::ProcessModel& proc,
                const DocumentPlugin& doc,
                const iscore::DocumentContext& ctx,
                QObject* paren_objt) const override;
};
}
}
