#pragma once
#include <ossia/editor/scenario/time_value.hpp>
#include <ossia/editor/state/state_element.hpp>
#include <Process/TimeValue.hpp>
#include <QObject>
#include <score/model/Identifier.hpp>
#include <memory>
#include <score/model/ComponentHierarchy.hpp>
#include <Scenario/Document/Components/IntervalComponent.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <score_plugin_engine_export.h>

Q_DECLARE_METATYPE(ossia::time_value)
namespace Process
{
class ProcessModel;
}
namespace score
{
struct DocumentContext;
}
namespace ossia
{
class loop;
class time_interval;
}
namespace Scenario
{
class IntervalModel;
}

namespace Engine
{
namespace Execution
{
class IntervalComponentBase;
class IntervalComponent;

}
}

namespace score
{
template<>
struct is_component_serializable<Engine::Execution::IntervalComponentBase>
{
    using type = score::not_serializable_tag;
};

template<>
struct is_component_serializable<Engine::Execution::IntervalComponent>
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

struct interval_duration_data
{
    ossia::time_value defaultDuration;
    ossia::time_value minDuration;
    ossia::time_value maxDuration;
    double speed;
};

class SCORE_PLUGIN_ENGINE_EXPORT IntervalComponentBase :
    public Scenario::GenericIntervalComponent<const Context>
{
    Q_OBJECT
    COMMON_COMPONENT_METADATA("4d644678-1924-49bf-8c82-89841581d23f")
    public:
      using parent_t = Engine::Execution::Component;
    using model_t = Process::ProcessModel;
    using component_t = ProcessComponent;
    using component_factory_list_t = ProcessComponentFactoryList;

    static const constexpr bool is_unique = true;
    IntervalComponentBase(
        Scenario::IntervalModel& score_cst,
        const Context& ctx,
        const Id<score::Component>& id,
        QObject* parent);
    IntervalComponentBase(const IntervalComponentBase&) = delete;
    IntervalComponentBase(IntervalComponentBase&&) = delete;
    IntervalComponentBase& operator=(const IntervalComponentBase&) = delete;
    IntervalComponentBase& operator=(IntervalComponentBase&&) = delete;


    //! To be called from the GUI thread
    interval_duration_data makeDurations() const;

    const std::shared_ptr<ossia::time_interval>& OSSIAInterval() const;
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

    std::shared_ptr<ossia::time_interval> m_ossia_interval;
    score::hash_map<Id<Process::ProcessModel>, std::shared_ptr<ProcessComponent>> m_processes;
};


class SCORE_PLUGIN_ENGINE_EXPORT IntervalComponent final :
    public score::PolymorphicComponentHierarchy<IntervalComponentBase, false>
{
    Q_OBJECT

  public:
    template<typename... Args>
    IntervalComponent(Args&&... args):
      PolymorphicComponentHierarchyManager{
        score::lazy_init_t{}, std::forward<Args>(args)...}
    {
      connect(this, &IntervalComponent::sig_callback,
              this, &IntervalComponent::slot_callback,
              Qt::QueuedConnection);
    }

    IntervalComponent(const IntervalComponent&) = delete;
    IntervalComponent(IntervalComponent&&) = delete;
    IntervalComponent& operator=(const IntervalComponent&) = delete;
    IntervalComponent& operator=(IntervalComponent&&) = delete;
    ~IntervalComponent();

    // only here to help autocompletion
    const std::shared_ptr<ossia::time_interval>& OSSIAInterval() const { return IntervalComponentBase::OSSIAInterval(); }

    void init();
    void cleanup(std::shared_ptr<IntervalComponent>);

    //! To be called from the API edition thread
    void onSetup(std::shared_ptr<IntervalComponent>,
                 std::shared_ptr<ossia::time_interval> ossia_cst,
                 interval_duration_data dur,
                 bool parent_is_base_scenario);

  signals:
    void sig_callback(double position, ossia::time_value date);
  public slots:
    void slot_callback(double position, ossia::time_value date);
};
}
}
