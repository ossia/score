#pragma once
#include <Engine/Node/Executor.hpp>
#include <Engine/Node/Layer.hpp>
#include <Engine/Node/Process.hpp>
#include <Media/Effect/Effect/EffectModel.hpp>
#include <Media/Effect/EffectExecutor.hpp>

////////// METADATA ////////////
namespace Control
{
template<typename Info, typename = Control::is_control>
class ControlEffect;
}
template <typename Info>
struct Metadata<PrettyName_k, Control::ControlEffect<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR auto get()
    {
      return Info::Metadata::prettyName;
    }
};
template <typename Info>
struct Metadata<Category_k, Control::ControlEffect<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR auto get()
    {
      return Info::Metadata::category;
    }
};
template <typename Info>
struct Metadata<Tags_k, Control::ControlEffect<Info>>
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
struct Metadata<ObjectKey_k, Control::ControlEffect<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR auto get()
    {
      return Info::Metadata::objectKey;
    }
};
template <typename Info>
struct Metadata<ConcreteKey_k, Control::ControlEffect<Info>>
{
    static Q_DECL_RELAXED_CONSTEXPR UuidKey<Media::Effect::EffectFactory> get()
    {
      return Info::Metadata::uuid;
    }
};


namespace Control
{
template<typename Info, typename>
class ControlEffect final: public Media::Effect::EffectModel
{
    SCORE_SERIALIZE_FRIENDS
    MODEL_METADATA_IMPL(ControlEffect<Info>)
    friend struct TSerializer<DataStream, Control::ControlEffect<Info>>;
    friend struct TSerializer<JSONObject, Control::ControlEffect<Info>>;
    friend struct Control::PortSetup;
  public:

    ossia::value control(std::size_t i) const
    {
      static_assert(InfoFunctions<Info>::control_count != 0);
      constexpr auto start = InfoFunctions<Info>::control_start;

      return static_cast<Process::ControlInlet*>(m_inlets[start + i])->value();
    }

    void setControl(std::size_t i, ossia::value v)
    {
      static_assert(InfoFunctions<Info>::control_count != 0);
      constexpr auto start = InfoFunctions<Info>::control_start;

      static_cast<Process::ControlInlet*>(m_inlets[start + i])->setValue(std::move(v));
    }

    auto& inlets_ref() const { return m_inlets; }
    auto& outlets_ref() const { return m_outlets; }

    ControlEffect(
        const Id<Media::Effect::EffectModel>& id,
        QObject* parent):
      Media::Effect::EffectModel{id, parent}
    {
      metadata().setInstanceName(*this);

      Control::PortSetup::init<Info>(*this);
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
};

template<typename Info>
struct ControlEffectView:  public Control::EffectItem
{
  public:
    ControlEffectView(ControlEffect<Info>& eff
                      , const score::DocumentContext& ctx,
                      QGraphicsItem* parent):
      Control::EffectItem{parent}
    {
      Control::UISetup::init<Info>(eff, *this, ctx);
    }
};
template<typename Info>
using ControlEffectUIFactory = Media::Effect::EffectUIFactory_T<ControlEffect<Info>, ControlEffectView<Info>>;
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
struct is_custom_serialized<Control::ControlEffect<Info>>: std::true_type { };


namespace score
{

template<typename Vis, typename Info>
void serialize_dyn_impl(Vis& v, const Control::ControlEffect<Info>& t)
{
  TSerializer<typename Vis::type, Control::ControlEffect<Info>>::readFrom(v, t);
}

}

namespace Engine
{
namespace Execution
{
template<typename Info>
class ControlEffectComponent final
    : public Engine::Execution::EffectComponent_T<Control::ControlEffect<Info>>
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
      Control::ControlEffect<Info>& proc,
      const Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent)
    : Engine::Execution::EffectComponent_T<Control::ControlEffect<Info>>{proc, ctx, id, parent}
  {
    auto node = std::make_shared<Control::ControlNode<Info>>();
    Control::setup_node<Info>(node, proc, ctx, &proc);
    this->node = node;
  }
};

template<typename Info>
using ControlEffectComponentFactory = Engine::Execution::EffectComponentFactory_T<ControlEffectComponent<Info>>;
}
}
