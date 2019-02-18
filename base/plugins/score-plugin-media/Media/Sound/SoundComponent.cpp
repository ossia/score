#include "SoundComponent.hpp"

#include <Process/ExecutionContext.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/sound.hpp>
#include <ossia/detail/pod_vector.hpp>

namespace Execution
{
using sound_proc_type = ossia::nodes::sound_ref;

SoundComponent::SoundComponent(
    Media::Sound::ProcessModel& element, const Execution::Context& ctx,
    const Id<score::Component>& id, QObject* parent)
    : Execution::ProcessComponent_T<
          Media::Sound::ProcessModel, ossia::node_process>{
          element, ctx, id, "Executor::SoundComponent", parent}
{
  auto node = std::make_shared<sound_proc_type>();
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  con(element, &Media::Sound::ProcessModel::fileChanged, this,
      [this] { this->recompute(); });
  con(element.file()->decoder(), &Media::AudioDecoder::finishedDecoding, this,
      [this] { this->recompute(); });
  con(element, &Media::Sound::ProcessModel::startChannelChanged, this, [=] {
    in_exec(
        [node, start = process().startChannel()] { node->set_start(start); });
  });
  con(element, &Media::Sound::ProcessModel::upmixChannelsChanged, this, [=] {
    in_exec(
        [node, upmix = process().upmixChannels()] { node->set_upmix(upmix); });
  });
  con(element, &Media::Sound::ProcessModel::startOffsetChanged, this, [=] {
    in_exec([node, off = process().startOffset()] {
      node->set_start_offset(off);
    });
  });
  recompute();
}

void SoundComponent::recompute()
{
  if constexpr (std::is_same_v<sound_proc_type, ossia::nodes::sound>)
  {
    auto to_double = [](const auto& float_vec) {
      std::vector<ossia::double_vector> v;
      v.reserve(float_vec.size());
      for (auto& chan : float_vec)
      {
        v.emplace_back(chan.begin(), chan.end());
      }
      return v;
    };
    in_exec(
        [n
         = std::dynamic_pointer_cast<ossia::nodes::sound>(OSSIAProcess().node),
         data = to_double(process().file()->data()),
         upmix = process().upmixChannels(), start = process().startChannel(),
         startOff = process().startOffset()]() mutable {
          n->set_sound(std::move(data));
          n->set_start(start);
          n->set_start_offset(startOff);
          n->set_upmix(upmix);
        });
  }
  else
  {
    in_exec([n = std::dynamic_pointer_cast<ossia::nodes::sound_ref>(
                 OSSIAProcess().node),
             data = process().file()->handle(),
             upmix = process().upmixChannels(),
             start = process().startChannel(),
             startOff = process().startOffset()] {
      n->set_sound(std::move(data));
      n->set_start(start);
      n->set_start_offset(startOff);
      n->set_upmix(upmix);
    });
  }
}

SoundComponent::~SoundComponent()
{
}
}
