#pragma once
#include <Media/Effect/Effect/EffectModel.hpp>
#include <QJsonDocument>
#include <Media/Effect/VST/VSTLoader.hpp>

namespace Media
{
namespace VST
{
class VSTEffectModel;
}
}
EFFECT_METADATA(, Media::VST::VSTEffectModel, "BE8E6BD3-75F2-4102-8895-8A4EB4EA545A", "VST", "VST")
namespace Media
{
namespace VST
{
class VSTEffectModel :
        public Effect::EffectModel
{
        Q_OBJECT
        SCORE_SERIALIZE_FRIENDS
        MODEL_METADATA_IMPL(VSTEffectModel)
    public:
        VSTEffectModel(
                const QString& name,
                const Id<EffectModel>&,
                QObject* parent);

        VSTEffectModel(
                const VSTEffectModel& source,
                const Id<EffectModel>&,
                QObject* parent);

        ~VSTEffectModel();
        template<typename Impl>
        VSTEffectModel(
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

        std::unique_ptr<VSTModule> plugin;
        AEffect* fx{};
    private:
        void reload();
        void showUI() override;
        void hideUI() override;
        QString m_effectPath;

        void readPlugin();

        auto dispatch(VstInt32 opcode, VstInt32 index = 0, VstIntPtr value = 0, void *ptr = nullptr, float opt = 0.0f)
        {
          return fx->dispatcher(fx, opcode, index, value, ptr, opt);
        }

        void closePlugin();
};
using VSTEffectFactory = Effect::EffectFactory_T<VSTEffectModel>;
}
}
