#pragma once
#include <Media/Effect/Effect/EffectModel.hpp>

namespace Media
{
namespace Effect
{
class EnvelopeEffectModel;
}
}
EFFECT_METADATA(, Media::Effect::EnvelopeEffectModel, "d66c938c-3239-435c-9bbb-04dc3bc0ffc5", "Envelope", "Envelope")
namespace Media
{
namespace Effect
{
class EnvelopeEffectModel :
        public EffectModel
{
        Q_OBJECT
        SCORE_SERIALIZE_FRIENDS
        MODEL_METADATA_IMPL(EnvelopeEffectModel)
    public:
        EnvelopeEffectModel(
                const QString& name,
                const Id<EffectModel>&,
                QObject* parent);

        EnvelopeEffectModel(
                const EnvelopeEffectModel& source,
                const Id<EffectModel>&,
                QObject* parent);

        template<typename Impl>
        EnvelopeEffectModel(
                Impl& vis,
                QObject* parent) :
            EffectModel{vis, parent}
        {
            vis.writeTo(*this);
            init();
        }

        void init();
};
using EnvelopeEffectFactory = EffectFactory_T<EnvelopeEffectModel>;
}
}
