#pragma once
#include <Media/Effect/Effect/EffectModel.hpp>
#include <QJsonDocument>
#include <Media/Effect/VST/VSTLoader.hpp>
#include <Media/Effect/EffectExecutor.hpp>
namespace Media
{
namespace VST
{
class VSTEffectModel;
}
}
EFFECT_METADATA(, Media::VST::VSTEffectModel, "BE8E6BD3-75F2-4102-8895-8A4EB4EA545A", "VST", "VST", "", {})
namespace Media
{
namespace VST
{
struct AEffectWrapper
{
    AEffect* fx{};

    AEffectWrapper(AEffect* f): fx{f}
    {

    }

    auto getParameter(VstInt32 index)
    {
      return fx->getParameter(fx, index);
    }
    auto setParameter(VstInt32 index, float p)
    {
      return fx->setParameter(fx, index, p);
    }

    auto dispatch(VstInt32 opcode, VstInt32 index = 0, VstIntPtr value = 0, void *ptr = nullptr, float opt = 0.0f)
    {
      return fx->dispatcher(fx, opcode, index, value, ptr, opt);
    }

    ~AEffectWrapper()
    {
      if(fx)
      {
        fx->dispatcher(fx, effStopProcess, 0, 0, nullptr, 0.f);
        fx->dispatcher(fx, effMainsChanged, 0, 0, nullptr, 0.f);
        fx->dispatcher(fx, effClose, 0, 0, nullptr, 0.f);
      }
    }
};
class VSTEffectModel :
        public Effect::EffectModel
{
        Q_OBJECT
        SCORE_SERIALIZE_FRIENDS
    public:
        MODEL_METADATA_IMPL(VSTEffectModel)
        VSTEffectModel(
                const QString& name,
                const Id<EffectModel>&,
                QObject* parent);

        VSTEffectModel(
                const VSTEffectModel& source,
                const Id<EffectModel>&,
                QObject* parent);

        ~VSTEffectModel() override;
        template<typename Impl>
        VSTEffectModel(
                Impl& vis,
                QObject* parent) :
            EffectModel{vis, parent}
        {
            vis.writeTo(*this);
        }

        const QString& effect() const
        { return m_effectPath; }

        void setEffect(const QString& s)
        { m_effectPath = s; }

        std::shared_ptr<AEffectWrapper> fx{};

        QString prettyName() const override;
        std::shared_ptr<ossia::audio_fx_node> makeNode(const Engine::Execution::Context &, QObject* ctx) override;
    private:
        void reload();
        void showUI() override;
        void hideUI() override;
        QString m_effectPath;

        auto dispatch(VstInt32 opcode, VstInt32 index = 0, VstIntPtr value = 0, void *ptr = nullptr, float opt = 0.0f)
        {
          return fx->dispatch(opcode, index, value, ptr, opt);
        }

        void closePlugin();
};
using VSTEffectFactory = Effect::EffectFactory_T<VSTEffectModel>;
}
}

namespace Engine
{
namespace Execution
{

class  VSTEffectComponent
    : public Engine::Execution::EffectComponent_T<Media::VST::VSTEffectModel>
{
  Q_OBJECT
  COMPONENT_METADATA("84bb8af9-bfb9-4819-8427-79787de716f3")

public:
    static constexpr bool is_unique = true;

  VSTEffectComponent(
      Media::VST::VSTEffectModel& proc,
      const Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent)
    : Engine::Execution::EffectComponent_T<Media::VST::VSTEffectModel>{proc, ctx, id, parent}
  {
    node = proc.makeNode(ctx, this);
  }
};
using VSTEffectComponentFactory = Engine::Execution::EffectComponentFactory_T<VSTEffectComponent>;
}
}
