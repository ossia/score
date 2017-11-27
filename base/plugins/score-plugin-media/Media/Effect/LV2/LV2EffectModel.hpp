#pragma once
#include <Media/Effect/Effect/EffectModel.hpp>
#include <lilv/lilvmm.hpp>
#include <Media/Effect/LV2/LV2Context.hpp>
#include <QJsonDocument>

namespace Media
{
namespace LV2
{
class LV2EffectModel;
}
}
EFFECT_METADATA(, Media::LV2::LV2EffectModel, "fd5243ba-70b5-4164-b44a-ecb0dcdc0494", "LV2", "LV2")
namespace Media
{
namespace LV2
{



/** LV2 effect model.
 * Should contain an effect, maybe instantiated with
 * LibMediaStream's MakeLV2MediaEffect
 * Cloning can be done with MakeCopyEffect.
 */
class LV2EffectModel :
        public Effect::EffectModel
{
        Q_OBJECT
        SCORE_SERIALIZE_FRIENDS
        MODEL_METADATA_IMPL(LV2EffectModel)
    public:
        LV2EffectModel(
                const QString& name,
                const Id<EffectModel>&,
                QObject* parent);

        LV2EffectModel(
                const LV2EffectModel& source,
                const Id<EffectModel>&,
                QObject* parent);

        template<typename Impl>
        LV2EffectModel(
                Impl& vis,
                QObject* parent) :
            EffectModel{vis, parent}
        {
            vis.writeTo(*this);
            reload();
        }

        const QString& effect() const
        { return m_effectPath; }

        void setEffect(const QString& s)
        { m_effectPath = s; }

        const LilvPlugin* plugin{};
        LV2::EffectContext effectContext;
    private:
        void reload();
        QString m_effectPath;
        void readPlugin();
};
using LV2EffectFactory = Effect::EffectFactory_T<LV2EffectModel>;
}
}
