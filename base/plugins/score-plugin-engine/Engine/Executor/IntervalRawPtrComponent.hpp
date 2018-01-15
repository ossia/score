#pragma once
#include <Engine/Executor/IntervalComponent.hpp>


namespace Engine
{
namespace Execution
{
class IntervalRawPtrComponentBase;
class IntervalRawPtrComponent;

}
}

namespace score
{
template<>
struct is_component_serializable<Engine::Execution::IntervalRawPtrComponentBase>
{
    using type = score::not_serializable_tag;
};

template<>
struct is_component_serializable<Engine::Execution::IntervalRawPtrComponent>
{
    using type = score::not_serializable_tag;
};
}

namespace Engine
{
namespace Execution
{
struct Context;
class DocumentPlugin;


class SCORE_PLUGIN_ENGINE_EXPORT IntervalRawPtrComponentBase :
    public Scenario::GenericIntervalComponent<const Context>
{
    COMMON_COMPONENT_METADATA("15f89c71-75a8-4966-8c3e-710a46f7a4db")
    public:
      using parent_t = Engine::Execution::Component;
    using model_t = Process::ProcessModel;
    using component_t = ProcessComponent;
    using component_factory_list_t = ProcessComponentFactoryList;

    static const constexpr bool is_unique = true;
    IntervalRawPtrComponentBase(
        Scenario::IntervalModel& score_cst,
        const Context& ctx,
        const Id<score::Component>& id,
        QObject* parent);
    IntervalRawPtrComponentBase(const IntervalRawPtrComponentBase&) = delete;
    IntervalRawPtrComponentBase(IntervalRawPtrComponentBase&&) = delete;
    IntervalRawPtrComponentBase& operator=(const IntervalRawPtrComponentBase&) = delete;
    IntervalRawPtrComponentBase& operator=(IntervalRawPtrComponentBase&&) = delete;


    //! To be called from the GUI thread
    interval_duration_data makeDurations() const;

    ossia::time_interval* OSSIAInterval() const;
    Scenario::IntervalModel& scoreInterval() const;

    const auto& processes() const { return m_processes; }

    void pause();
    void resume();
    void stop();

    void executionStarted();
    void executionStopped();


    ProcessComponent* make(
        const Id<score::Component> & id,
        ProcessComponentFactory& factory,
        Process::ProcessModel &process);
    std::function<void()> removing(
        const Process::ProcessModel& e,
        ProcessComponent& c);

    template<typename... Args>
    void added(Args&&...) { }
    template <typename Component_T, typename Element, typename Fun>
    void removed(const Element& elt, const Component_T& comp, Fun f)
    {
      if(f)
        f();
    }

    const Context& context() const { return system(); }
  protected:
    void on_processAdded(Process::ProcessModel& score_proc);

    ossia::time_interval* m_ossia_interval{};
    score::hash_map<Id<Process::ProcessModel>, std::shared_ptr<ProcessComponent>> m_processes;
};


class SCORE_PLUGIN_ENGINE_EXPORT IntervalRawPtrComponent final :
    public score::PolymorphicComponentHierarchy<IntervalRawPtrComponentBase, false>
{
    Q_OBJECT

  public:
    template<typename... Args>
    IntervalRawPtrComponent(Args&&... args):
      PolymorphicComponentHierarchyManager{
        score::lazy_init_t{}, std::forward<Args>(args)...}
    {
      connect(this, &IntervalRawPtrComponent::sig_callback,
              this, &IntervalRawPtrComponent::slot_callback,
              Qt::QueuedConnection);
    }

    IntervalRawPtrComponent(const IntervalRawPtrComponent&) = delete;
    IntervalRawPtrComponent(IntervalRawPtrComponent&&) = delete;
    IntervalRawPtrComponent& operator=(const IntervalRawPtrComponent&) = delete;
    IntervalRawPtrComponent& operator=(IntervalRawPtrComponent&&) = delete;
    ~IntervalRawPtrComponent();

    // only here to help autocompletion
    ossia::time_interval* OSSIAInterval() const { return IntervalRawPtrComponentBase::OSSIAInterval(); }

    void init();
    void cleanup(const std::shared_ptr<IntervalRawPtrComponent>& self);

    //! To be called from the API edition thread
    void onSetup(
        std::shared_ptr<IntervalRawPtrComponent> self,
        ossia::time_interval* ossia_cst,
        interval_duration_data dur,
        bool parent_is_base_scenario);
  Q_SIGNALS:
    void sig_callback(double position, ossia::time_value date);
  public Q_SLOTS:
    void slot_callback(double position, ossia::time_value date);
};
}
}
