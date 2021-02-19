#include "SoundComponent.hpp"

#include <Process/ExecutionContext.hpp>
#include <Process/ExecutionSetup.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/dummy.hpp>
#include <ossia/dataflow/nodes/sound.hpp>
#include <ossia/dataflow/nodes/sound_mmap.hpp>
#include <ossia/dataflow/nodes/sound_ref.hpp>
#include <ossia/detail/pod_vector.hpp>

namespace
{

static std::unique_ptr<ossia::resampler> make_resampler(const Media::Sound::ProcessModel& element) noexcept
{
  auto res = std::make_unique<ossia::resampler>();
  const auto channels = element.file()->channels();
  const auto sampleRate = element.file()->sampleRate();
  res->reset(0, element.stretchMode(), channels, sampleRate);
  return res;
}

}
namespace Media
{

class SoundComponentSetup
{
public:
  static void construct(Execution::SoundComponent& component)
  {
    Sound::ProcessModel& element = component.process();
    auto handle = element.file();

    struct
    {
      Execution::SoundComponent& component;
      void operator()() const noexcept
      {
        // Note : we need to construct a dummy node in case there is nothing,
        // because else the parent interval will destroy the component so
        // even if the sound is created afterwards there won't be any component
        // anymore

        auto node = std::make_shared<ossia::dummy_sound_node>();
        component.node = node;
        component.m_ossia_process = std::make_shared<ossia::sound_process>(node);
      }
      void operator()(const std::shared_ptr<Media::AudioFile::LibavReader>& r) const noexcept
      {
        Execution::Transaction commands{component.system()};

        auto node = std::make_shared<ossia::nodes::sound_ref>();
        component.node = node;
        component.m_ossia_process = std::make_shared<ossia::sound_process>(node);
        update_ref(node, r, component, commands);

        commands.run_all();
      }
      void operator()(const Media::AudioFile::MmapReader& r) const noexcept
      {
        Execution::Transaction commands{component.system()};

        auto node = std::make_shared<ossia::nodes::sound_mmap>();
        component.node = node;
        component.m_ossia_process = std::make_shared<ossia::sound_process>(node);
        update_mmap(node, r, component, commands);

        commands.run_all();
      }
    } _{component};

    ossia::apply(_, handle->m_impl);
  }

  static void update(Execution::SoundComponent& component)
  {
    Sound::ProcessModel& element = component.process();
    auto handle = element.file();

    struct
    {
      Execution::SoundComponent& component;
      void operator()() const noexcept { return; }
      void operator()(const std::shared_ptr<Media::AudioFile::LibavReader>& r) const noexcept
      {
        Execution::Transaction commands{component.system()};
        auto old_node = component.node;

        if(auto n = std::dynamic_pointer_cast<ossia::nodes::sound_ref>(old_node))
        {
          update_ref(n, r, component, commands);
        }
        else
        {
          n = std::make_shared<ossia::nodes::sound_ref>();
          update_ref(n, r, component, commands);
          component.system().setup.unregister_node(component.process(), old_node, commands);
          component.system().setup.register_node(component.process(), component.node, commands);
          component.system().setup.replace_node(component.OSSIAProcessPtr(), component.node, commands);
          component.nodeChanged(old_node, component.node, commands);
        }

        commands.run_all();
      }
      void operator()(const Media::AudioFile::MmapReader& r) const noexcept
      {
        if(!r.wav)
          return;

        Execution::Transaction commands{component.system()};
        auto old_node = component.node;

        if(auto n = std::dynamic_pointer_cast<ossia::nodes::sound_mmap>(old_node))
        {
          update_mmap(n, r, component, commands);
        }
        else
        {
          n = std::make_shared<ossia::nodes::sound_mmap>();
          update_mmap(n, r, component, commands);
          component.system().setup.unregister_node(component.process(), old_node, commands);
          component.system().setup.register_node(component.process(), component.node, commands);
          component.system().setup.replace_node(component.OSSIAProcessPtr(), component.node, commands);
          component.nodeChanged(old_node, component.node, commands);
        }

        commands.run_all();
      }
    } _{component};

    ossia::apply(_, handle->m_impl);
  }

  static void update_ref(
      const std::shared_ptr<ossia::nodes::sound_ref>& n,
      const std::shared_ptr<Media::AudioFile::LibavReader>& r,
      Execution::SoundComponent& component,
      Execution::Transaction& commands)
  {
    auto& p = component.process();
    commands.push_back([n,
                       data = r->handle,
                       channels = r->decoder.channels,
                       sampleRate = r->decoder.convertedSampleRate,
                       tempo = component.process().nativeTempo(),
                       res = make_resampler(component.process()),
                       upmix = p.upmixChannels(),
                       start = p.startChannel()] () mutable{
      n->set_sound(std::move(data), channels, sampleRate);
      n->set_start(start);
      n->set_upmix(upmix);
      n->set_resampler(std::move(*res));
      n->set_native_tempo(tempo);
    });
  }

  static void
  update_mmap(
      const std::shared_ptr<ossia::nodes::sound_mmap>& n,
      const Media::AudioFile::MmapReader& r,
      Execution::SoundComponent& component,
      Execution::Transaction& commands)
  {
    auto& p = component.process();
    commands.push_back(
          [n,
          data = r.wav,
          channels = r.wav.channels(),
          tempo = component.process().nativeTempo(),
          res = make_resampler(component.process()),
          upmix = p.upmixChannels(),
          start = p.startChannel()]() mutable {
      n->set_sound(std::move(data));
      n->set_start(start);
      n->set_upmix(upmix);
      n->set_resampler(std::move(*res));
      n->set_native_tempo(tempo);
    });
  }
};
}
namespace Execution
{

SoundComponent::SoundComponent(
    Media::Sound::ProcessModel& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : Execution::ProcessComponent_T<
        Media::Sound::ProcessModel,
        ossia::node_process>{element, ctx, id, "Executor::SoundComponent", parent}
    , m_recomputer{*this}
{
  Media::SoundComponentSetup{}.construct(*this);
  connect(
      &element, &Media::Sound::ProcessModel::fileChanged, this, &SoundComponent::on_fileChanged);

  auto node_action = [this](auto&& f) {
    if (auto n_ref = std::dynamic_pointer_cast<ossia::nodes::sound_ref>(this->node))
      in_exec([n_ref, ff=std::move(f)] () mutable { ff(*n_ref); });
    else if (auto n_mmap = std::dynamic_pointer_cast<ossia::nodes::sound_mmap>(this->node))
      in_exec([n_mmap, ff=std::move(f)] () mutable { ff(*n_mmap); });
  };

  con(element, &Media::Sound::ProcessModel::startChannelChanged, this, [=, &element] {
    node_action([start = element.startChannel()](auto& node) { node.set_start(start); });
  });
  con(element, &Media::Sound::ProcessModel::upmixChannelsChanged, this, [=, &element] {
    node_action([start = element.upmixChannels()](auto& node) { node.set_upmix(start); });
  });
  con(element, &Media::Sound::ProcessModel::nativeTempoChanged, this, [=, &element] {
    node_action([start = element.nativeTempo()](auto& node) { node.set_native_tempo(start); });
  });
  con(element, &Media::Sound::ProcessModel::stretchModeChanged, this, [=, &element] {
    node_action([r = make_resampler(element)](auto& node) mutable {
      node.set_resampler(std::move(*r));
    });
  });

  if (auto& file = element.file())
  {
    file->on_finishedDecoding.connect<&SoundComponent::Recomputer::recompute>(m_recomputer);
  }
}
void SoundComponent::on_fileChanged()
{
  m_recomputer.~Recomputer();
  new (&m_recomputer) Recomputer{*this};

  recompute();

  if (auto& file = process().file())
  {
    file->on_finishedDecoding.connect<&SoundComponent::Recomputer::recompute>(m_recomputer);
  }
}

void SoundComponent::recompute()
{
  Media::SoundComponentSetup{}.update(*this);
}

SoundComponent::~SoundComponent() { }
}
