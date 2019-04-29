#include "SoundComponent.hpp"

#include <Process/ExecutionContext.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/sound.hpp>
#include <ossia/dataflow/nodes/sound_ref.hpp>
#include <ossia/dataflow/nodes/sound_mmap.hpp>
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
      void operator()(const std::monostate& r) const noexcept
      { return ; }
      void operator()(const std::shared_ptr<Media::FFMPEGAudioFileHandle::LibavReader>& r) const noexcept
      {
        construct_ffmpeg(r, component);
      }
      void operator()(const std::shared_ptr<Media::FFMPEGAudioFileHandle::MmapReader>& r) const noexcept
      {
        construct_drwav(r, component);
      }
    } _{component};

    std::visit(_, handle->m_impl);
  }

  static void construct_ffmpeg(
      const std::shared_ptr<Media::FFMPEGAudioFileHandle::LibavReader>& r,
      Execution::SoundComponent& component)
  {
    auto node = std::make_shared<ossia::nodes::sound_ref>();
    component.node = node;
    component.m_ossia_process = std::make_shared<ossia::node_process>(node);

    auto& element = component.process();

    con(element, &Media::Sound::ProcessModel::fileChanged,
        &component, &Execution::SoundComponent::recompute);
    element.file()->on_finishedDecoding.connect<&Execution::SoundComponent::recompute>(component);

    con(element, &Media::Sound::ProcessModel::startChannelChanged, &component, [=,&element,&component] {
      component.in_exec([node, start = element.startChannel()] { node->set_start(start); });
    });
    con(element, &Media::Sound::ProcessModel::upmixChannelsChanged, &component, [=,&element,&component] {
      component.in_exec([node, upmix = element.upmixChannels()] { node->set_upmix(upmix); });
    });
    con(element, &Media::Sound::ProcessModel::startOffsetChanged, &component, [=,&element,&component] {
      component.in_exec([node, off = element.startOffset()] { node->set_start_offset(off); });
    });

    recompute_ffmpeg(r, component);
  }

  static void construct_drwav(
      const std::shared_ptr<Media::FFMPEGAudioFileHandle::MmapReader>& r,
      Execution::SoundComponent& component)
  {
    auto node = std::make_shared<ossia::nodes::sound_mmap>();
    component.node = node;
    component.m_ossia_process = std::make_shared<ossia::node_process>(node);

    auto& element = component.process();

    con(element, &Media::Sound::ProcessModel::fileChanged,
        &component, &Execution::SoundComponent::recompute);
    element.file()->on_finishedDecoding.connect<&Execution::SoundComponent::recompute>(component);

    con(element, &Media::Sound::ProcessModel::startChannelChanged, &component, [=,&element,&component] {
      component.in_exec([node, start = element.startChannel()] { node->set_start(start); });
    });
    con(element, &Media::Sound::ProcessModel::upmixChannelsChanged, &component, [=,&element,&component] {
      component.in_exec([node, upmix = element.upmixChannels()] { node->set_upmix(upmix); });
    });
    con(element, &Media::Sound::ProcessModel::startOffsetChanged, &component, [=,&element,&component] {
      component.in_exec([node, off = element.startOffset()] { node->set_start_offset(off); });
    });

    recompute_drwav(r, component);
  }


  static void recompute(Execution::SoundComponent& component)
  {
    Sound::ProcessModel& element = component.process();
    auto handle = element.file();

    struct
    {
      Execution::SoundComponent& component;
      void operator()(const std::monostate& r) const noexcept
      { return ; }
      void operator()(const std::shared_ptr<Media::FFMPEGAudioFileHandle::LibavReader>& r) const noexcept
      {
        recompute_ffmpeg(r, component);
      }
      void operator()(const std::shared_ptr<Media::FFMPEGAudioFileHandle::MmapReader>& r) const noexcept
      {
        recompute_drwav(r, component);
      }
    } _{component};

    std::visit(_, handle->m_impl);

  }
  static void recompute_ffmpeg(
      const std::shared_ptr<Media::FFMPEGAudioFileHandle::LibavReader>& r,
      Execution::SoundComponent& component)
  {
    auto& p = component.process();
    component.in_exec([n = std::dynamic_pointer_cast<ossia::nodes::sound_ref>(
                 component.OSSIAProcess().node),
             data = r->handle,
             upmix = p.upmixChannels(),
             start = p.startChannel(),
             startOff = p.startOffset()] {
      n->set_sound(std::move(data));
      n->set_start(start);
      n->set_start_offset(startOff);
      n->set_upmix(upmix);
    });
  }
  static void recompute_drwav(
      const std::shared_ptr<Media::FFMPEGAudioFileHandle::MmapReader>& r,
      Execution::SoundComponent& component)
  {
    Sound::ProcessModel& p = component.process();

    component.in_exec([n = std::dynamic_pointer_cast<ossia::nodes::sound_mmap>(
                 component.OSSIAProcess().node),
             data = r->wav,
             upmix = p.upmixChannels(),
             start = p.startChannel(),
             startOff = p.startOffset()] {
      n->set_sound(std::move(data));
      n->set_start(start);
      n->set_start_offset(startOff);
      n->set_upmix(upmix);
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
    : Execution::
          ProcessComponent_T<Media::Sound::ProcessModel, ossia::node_process>{
              element,
              ctx,
              id,
              "Executor::SoundComponent",
              parent}
{
  Media::SoundComponentSetup{}.construct(*this);
}

void SoundComponent::recompute()
{
  Media::SoundComponentSetup{}.recompute(*this);
}

SoundComponent::~SoundComponent() {}
}
