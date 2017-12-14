#pragma once
#include <Process/Process.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <Media/Effect/EffectProcessMetadata.hpp>
#include <score/model/EntityMap.hpp>
#include <Media/Effect/Effect/EffectModel.hpp>
namespace Media
{
namespace Effect
{
class ProcessModel;
/**
 * @brief The Media::Effect::ProcessModel class
 *
 * This class represents an effect chain.
 * Each effect should provide a component that will create
 * the corresponding LibMediaStream effect.
 *
 * Chaining two effects blocks [A] -> [B] is akin to
 * doing :
 *
 * MakeEffectSound(MakeEffectSound(Original sound, A, 0, 0), B, 0, 0)
 *
 */
class ProcessModel final : public Process::ProcessModel
{
        SCORE_SERIALIZE_FRIENDS
        PROCESS_METADATA_IMPL(Media::Effect::ProcessModel)

        Q_OBJECT
    public:
        explicit ProcessModel(
                const TimeVal& duration,
                const Id<Process::ProcessModel>& id,
                QObject* parent);

        explicit ProcessModel(
                const ProcessModel& source,
                const Id<Process::ProcessModel>& id,
                QObject* parent);

        ~ProcessModel() override;

        template<typename Impl>
        explicit ProcessModel(
                Impl& vis,
                QObject* parent) :
            Process::ProcessModel{vis, parent}
        {
            vis.writeTo(*this);
        }

        const score::EntityMap<EffectModel>& effects() const
        { return m_effects; }
        const auto& effectsOrder() const
        { return m_effectOrder; }

        void insertEffect(EffectModel* eff, int pos);
        void removeEffect(const Id<EffectModel>&);
        void moveEffect(const Id<EffectModel>&, int new_pos);

        int effectPosition(const Id<EffectModel>& e) const;

        Process::Inlets inlets() const override
        {
          return {inlet.get()};
        }

        Process::Outlets outlets() const override
        {
          return {outlet.get()};
        }

        std::unique_ptr<Process::Inlet> inlet{};
        std::unique_ptr<Process::Outlet> outlet{};

    signals:
        void effectsChanged();

    private:
        // The actual effect instances
        score::EntityMap<EffectModel> m_effects;

        // The effect chain.
        QList<Id<EffectModel>> m_effectOrder;
};
}
}
