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
      void operator()() const noexcept { construct_dummy(component); }
      void operator()(const std::shared_ptr<Media::AudioFile::LibavReader>& r) const noexcept
      {
        construct_ffmpeg(r, component);
      }
      void operator()(const Media::AudioFile::MmapReader& r) const noexcept
      {
        construct_drwav(r, component);
      }
    } _{component};

    ossia::apply(_, handle->m_impl);
  }

  // Note : we need to construct a dummy node in case there is nothing,
  // because else the parent interval will destroy the component so
  // even if the sound is created afterwards there won't be any component
  // anymore
  static void construct_dummy(Execution::SoundComponent& component)
  {
    auto node = std::make_shared<ossia::nodes::dummy_node>();
    component.node = node;
    if (component.m_ossia_process)
      component.m_ossia_process->node = node;
    else
      component.m_ossia_process = std::make_shared<ossia::node_process>(node);
  }

  static void construct_ffmpeg(
      const std::shared_ptr<Media::AudioFile::LibavReader>& r,
      Execution::SoundComponent& component)
  {
    auto node = std::make_shared<ossia::nodes::sound_ref>();
    component.node = node;
    if (component.m_ossia_process)
      component.m_ossia_process->node = node;
    else
      component.m_ossia_process = std::make_shared<ossia::sound_process>(node);

    recompute_ffmpeg(r, component);
  }

  static void
  construct_drwav(const Media::AudioFile::MmapReader& r, Execution::SoundComponent& component)
  {
    auto node = std::make_shared<ossia::nodes::sound_mmap>();
    component.node = node;
    if (component.m_ossia_process)
      component.m_ossia_process->node = node;
    else
      component.m_ossia_process = std::make_shared<ossia::sound_process>(node);

    recompute_drwav(r, component);
  }

  static void recompute(Execution::SoundComponent& component)
  {
    Sound::ProcessModel& element = component.process();
    auto handle = element.file();

    struct
    {
      Execution::SoundComponent& component;
      void operator()() const noexcept { return; }
      void operator()(const std::shared_ptr<Media::AudioFile::LibavReader>& r) const noexcept
      {
        recompute_ffmpeg(r, component);
      }
      void operator()(const Media::AudioFile::MmapReader& r) const noexcept
      {
        recompute_drwav(r, component);
      }
    } _{component};

    ossia::apply(_, handle->m_impl);
  }
  static void recompute_ffmpeg(
      const std::shared_ptr<Media::AudioFile::LibavReader>& r,
      Execution::SoundComponent& component)
  {
    auto& p = component.process();
    auto old_node = component.node;
    auto n = std::dynamic_pointer_cast<ossia::nodes::sound_ref>(old_node);
    if (n)
    {
      component.in_exec([n,
                         data = r->handle,
                         channels = r->decoder.channels,
                         sampleRate = r->decoder.sampleRate,
                         tempo = component.process().nativeTempo(),
                         mode = component.process().stretchMode(),
                         upmix = p.upmixChannels(),
                         start = p.startChannel()] {
        n->set_sound(std::move(data), channels, sampleRate);
        n->set_start(start);
        n->set_upmix(upmix);
        n->set_stretch_mode(mode);
        n->set_native_tempo(tempo);
      });
    }
    else
    {
      construct_ffmpeg(r, component);
      Execution::Transaction commands{component.system()};
      component.system().setup.unregister_node(component.process(), old_node, commands);
      component.system().setup.register_node(component.process(), component.node, commands);
      component.nodeChanged(old_node, component.node, commands);

      commands.run_all();
    }
  }
  static void
  recompute_drwav(const Media::AudioFile::MmapReader& r, Execution::SoundComponent& component)
  {
    if (!r.wav)
      return;
    Sound::ProcessModel& p = component.process();

    auto old_node = component.node;
    auto n = std::dynamic_pointer_cast<ossia::nodes::sound_mmap>(old_node);
    if (n)
    {
      component.in_exec(
          [n = std::dynamic_pointer_cast<ossia::nodes::sound_mmap>(component.OSSIAProcess().node),
           data = r.wav,
           channels = r.wav.channels(),
           tempo = component.process().nativeTempo(),
           mode = component.process().stretchMode(),
           upmix = p.upmixChannels(),
           start = p.startChannel()]() mutable {
            n->set_sound(std::move(data));
            n->set_start(start);
            n->set_upmix(upmix);
            n->set_stretch_mode(mode);
            n->set_native_tempo(tempo);
          });
    }
    else
    {
      construct_drwav(r, component);
      Execution::Transaction commands{component.system()};
      component.system().setup.unregister_node(component.process(), old_node, commands);
      component.system().setup.register_node(component.process(), component.node, commands);
      component.nodeChanged(old_node, component.node, commands);

      commands.run_all();
    }
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

  auto node_action = [this](auto f) {
    if (auto node = std::dynamic_pointer_cast<ossia::nodes::sound_ref>(this->node))
      in_exec([node, f] { f(*node); });
    else if (auto node = std::dynamic_pointer_cast<ossia::nodes::sound_mmap>(this->node))
      in_exec([node, f] { f(*node); });
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
    node_action([start = element.stretchMode()](auto& node) { node.set_stretch_mode(start); });
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
  Media::SoundComponentSetup{}.recompute(*this);
}

SoundComponent::~SoundComponent() { }
}
