#pragma once
#include <Engine/Node/Executor.hpp>
#include <Engine/Node/Layer.hpp>
#include <Engine/Node/Process.hpp>
#include <Media/Effect/Effect/EffectModel.hpp>

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

      constexpr const auto control_start = InfoFunctions<Info>::control_start;
      constexpr const auto control_count = InfoFunctions<Info>::control_count;

      if constexpr(control_count > 0)
      {
        for(std::size_t i = 0; i < control_count; i++)
        {
          node->controls[i] = control(i);
        }

        con(ctx.doc.coarseUpdateTimer, &QTimer::timeout,
            obj_ctx, [&,node] {
          typename ControlNode<Info>::controls_list arr;
          bool ok = false;
          while(node->cqueue.try_dequeue(arr)) {
            ok = true;
          }
          if(ok)
          {
            for(std::size_t i = 0; i < control_count; i++)
            {
              setControl(i, arr[i]);
            }
          }
        }, Qt::QueuedConnection);
      }

      for(std::size_t idx = control_start; idx < control_start + control_count; idx++)
      {
        auto inlet = static_cast<ControlInlet*>(inlets_ref()[idx]);
        //auto port = node->inputs()[idx]->data.template target<ossia::value_port>();

        QObject::connect(inlet, &ControlInlet::valueChanged,
                obj_ctx, [=,&ctx] (const ossia::value& val) {
          //this->system().executionQueue.enqueue(value_adder{*port, val});
          ctx.executionQueue.enqueue(control_updater{node->controls[idx - control_start], val});

        });
      }
      return node;
    }

    QGraphicsItem* makeItem(const score::DocumentContext& ctx) override
    {
      auto item = new Process::RectItem{};
      auto txt = new Scenario::SimpleTextItem{item};
      txt->setText(Metadata<PrettyName_k, ControlEffect<Info>>::get());
      txt->setPos(8., 4.);
      Process::UISetup::init<Info>(*this, *item, ctx, 20.);
      item->setRect({0., 0., 180., item->childrenBoundingRect().height() + 10.});
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

