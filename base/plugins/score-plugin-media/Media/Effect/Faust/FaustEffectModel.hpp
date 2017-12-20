#pragma once
#include <Effect/EffectModel.hpp>
#include <Effect/EffectFactory.hpp>
#include <QJsonDocument>
namespace Media
{
namespace Effect
{
class FaustEffectModel;
}
}
EFFECT_METADATA(, Media::Effect::FaustEffectModel, "5354c61a-1649-4f59-b952-5c2f1b79c1bd", "Faust", "Faust")

namespace Media
{
namespace Effect
{
/** Faust effect model.
 * Should contain an effect, maybe instantiated with
 * LibMediaStream's MakeFaustMediaEffect
 * Cloning can be done with MakeCopyEffect.
 */
class FaustEffectModel :
        public EffectModel
{
        Q_OBJECT
        SCORE_SERIALIZE_FRIENDS
        MODEL_METADATA_IMPL(FaustEffectModel)

    public:
        FaustEffectModel(
                const QString& faustProgram,
                const Id<EffectModel>&,
                QObject* parent);

        template<typename Impl>
        FaustEffectModel(
                Impl& vis,
                QObject* parent) :
            EffectModel{vis, parent}
        {
            vis.writeTo(*this);
            init();
        }

        const QString& text() const
        {
            return m_text;
        }

        void setText(const QString& txt);
        void showUI() override;
        void hideUI() override;

    private:
        void init();
        void reload();
        QString m_text;
};

using FaustEffectFactory = EffectFactory_T<FaustEffectModel>;
}
}
