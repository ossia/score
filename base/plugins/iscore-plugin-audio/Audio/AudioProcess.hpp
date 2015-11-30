#pragma once
#include <DocumentPlugin/ProcessModel/ProcessModelElement.hpp>
#include <ProcessModel/TimeProcessWithConstraint.hpp>
#include <Editor/TimeValue.h>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Audio/AudioBlock.hpp>
#include <Audio/AudioEngine.hpp>
#include <State/Value.hpp>
class DeviceDocumentPlugin;
class DeviceList;



namespace Audio
{
class Process final : public TimeProcessWithConstraint
{
    public:
        Process(AudioEngine& conf);

        void setAudioBlock(std::unique_ptr<AudioBlock>&& block);

        std::shared_ptr<OSSIA::StateElement> state(
                const OSSIA::TimeValue& t,
                const OSSIA::TimeValue&) override;


        const std::shared_ptr<OSSIA::State>& getStartState() const override;
        const std::shared_ptr<OSSIA::State>& getEndState() const override;

        auto block() const
        { return m_block.get(); }

        auto& engine() const
        { return m_conf; }

    private:
        AudioEngine& m_conf;
        std::unique_ptr<AudioBlock> m_block;

        std::shared_ptr<OSSIA::State> m_start;
        std::shared_ptr<OSSIA::State> m_end;
};

}
