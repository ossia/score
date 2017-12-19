#pragma once
#include <Engine/Node/Executor.hpp>
#include <Engine/Node/Layer.hpp>
#include <Engine/Node/Process.hpp>
#include <Media/Effect/Effect/EffectModel.hpp>
#include <Media/Effect/EffectExecutor.hpp>

////////// METADATA ////////////
namespace Process
{
template<typename Info, typename = Process::is_control>
class ControlEffect;
}
template <typename Info>
struct Metadata<PrettyName_k, Process::ControlEffect<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR auto get()
    {
      return Info::Metadata::prettyName;
    }
};
template <typename Info>
struct Metadata<Category_k, Process::ControlEffect<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR auto get()
    {
      return Info::Metadata::category;
    }
};
template <typename Info>
struct Metadata<Tags_k, Process::ControlEffect<Info>>
{
    static QStringList get()
    {
      QStringList lst;
      for(auto str : Info::Metadata::tags)
        lst.append(str);
      return lst;
    }
};
template <typename Info>
struct Metadata<ObjectKey_k, Process::ControlEffect<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR auto get()
    {
      return Info::Metadata::objectKey;
    }
};
template <typename Info>
struct Metadata<ConcreteKey_k, Process::ControlEffect<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR UuidKey<Media::Effect::EffectFactory> get()
    {
      return Info::Metadata::uuid;
    }
};


namespace Process
{
template<typename Info, typename>
class ControlEffect final: public Media::Effect::EffectModel
{
    SCORE_SERIALIZE_FRIENDS
    MODEL_METADATA_IMPL(ControlEffect<Info>)
    friend struct TSerializer<DataStream, Process::ControlEffect<Info>>;
    friend struct TSerializer<JSONObject, Process::ControlEffect<Info>>;
    friend struct Process::PortSetup;
  public:

    ossia::value control(std::size_t i) const
    {
      static_assert(InfoFunctions<Info>::control_count != 0);
      constexpr auto start = InfoFunctions<Info>::control_start;

      return static_cast<ControlInlet*>(m_inlets[start + i])->value();
    }

    void setControl(std::size_t i, ossia::value v)
    {
      static_assert(InfoFunctions<Info>::control_count != 0);
      constexpr auto start = InfoFunctions<Info>::control_start;

      static_cast<ControlInlet*>(m_inlets[start + i])->setValue(std::move(v));
    }

    auto& inlets_ref() const { return m_inlets; }
    auto& outlets_ref() const { return m_outlets; }

    ControlEffect(
        const Id<Media::Effect::EffectModel>& id,
        QObject* parent):
      Media::Effect::EffectModel{id, parent}
    {
      metadata().setInstanceName(*this);

      Process::PortSetup::init<Info>(*this);
    }

    ControlEffect(
        const ControlEffect& source,
        const Id<Media::Effect::EffectModel>& id,
        QObject* parent):
      Media::Effect::EffectModel{
        source,
        id,
        parent}
    {
      metadata().setInstanceName(*this);

      Process::PortSetup::clone<Info>(*this, source);
    }


    template<typename Impl>
    explicit ControlEffect(
        Impl& vis,
        QObject* parent) :
      Media::Effect::EffectModel{vis, parent}
    {
      vis.writeTo(*this);
    }

    ~ControlEffect() override
    {

    }

    QString prettyName() const override
    {
      return Metadata<PrettyName_k, ControlEffect<Info>>::get();
    }

    std::shared_ptr<ossia::audio_fx_node> makeNode(const Engine::Execution::Context& ctx, QObject* obj_ctx) override
    {
      auto node = std::make_shared<Process::ControlNode<Info>>();

      setup_node<Info>(node, *this, ctx, this);

      return node;
    }

    Process::EffectItem* makeItem(const score::DocumentContext& ctx) override
    {
      auto item = new Process::EffectItem{};
      Process::UISetup::init<Info>(*this, *item, ctx);
      return item;
    }

};

template<typename Info>
class ControlEffectFactory final :
        public Media::Effect::EffectFactory
{
    public:
        virtual ~ControlEffectFactory() = default;

        static auto static_concreteKey()
        { return Metadata<ConcreteKey_k, ControlEffect<Info>>::get(); }
    private:
        UuidKey<Media::Effect::EffectFactory> concreteKey() const noexcept override
        { return Metadata<ConcreteKey_k, ControlEffect<Info>>::get(); }

        QString prettyName() const override
        { return Metadata<PrettyName_k, ControlEffect<Info>>::get(); }
        QString category() const override
        { return Metadata<Category_k, ControlEffect<Info>>::get(); }

        ControlEffect<Info>* make(
                const QString& info, // plugin name ? faust code ? dll location ?
                const Id<Media::Effect::EffectModel>& id,
                QObject* parent) const final override
        {
            return new ControlEffect<Info>{id, parent};
        }

        ControlEffect<Info>* load(
                const VisitorVariant& vis,
                QObject* parent) const final override
        {
            return score::deserialize_dyn(vis, [&] (auto&& deserializer)
            { return new ControlEffect<Info>{deserializer, parent}; });
        }
};

}

template<typename Info>
struct is_custom_serialized<Process::ControlEffect<Info>>: std::true_type { };


namespace score
{

template<typename Vis, typename Info>
void serialize_dyn_impl(Vis& v, const Process::ControlEffect<Info>& t)
{
  TSerializer<typename Vis::type, Process::ControlEffect<Info>>::readFrom(v, t);
}

}

namespace Engine
{
namespace Execution
{
template<typename Info>
class ControlEffectComponent final
    : public Engine::Execution::EffectComponent_T<Process::ControlEffect<Info>>
{
  public:
    static Q_DECL_RELAXED_CONSTEXPR score::Component::Key static_key()
    {
      return Info::Metadata::uuid;
    }

    score::Component::Key key() const final override
    {
      return static_key();
    }

    bool key_match(score::Component::Key other) const final override
    {
      return static_key() == other
             || Engine::Execution::ProcessComponent::base_key_match(other);
    }
    static constexpr bool is_unique = true;

  ControlEffectComponent(
      Process::ControlEffect<Info>& proc,
      const Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent)
    : Engine::Execution::EffectComponent_T<Process::ControlEffect<Info>>{proc, ctx, id, parent}
  {
    this->node = proc.makeNode(ctx, this);
  }
};

template<typename Info>
using ControlEffectComponentFactory = Engine::Execution::EffectComponentFactory_T<ControlEffectComponent<Info>>;
}
}
