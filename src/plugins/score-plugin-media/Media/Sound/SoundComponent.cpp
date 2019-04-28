#include "SoundComponent.hpp"

#include <Process/ExecutionContext.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/nodes/sound.hpp>
#include <ossia/detail/pod_vector.hpp>

namespace Execution
{
using sound_proc_type = ossia::nodes::sound_ref;
}
namespace Media
{
class SoundComponentSetup
{
public:

  void construct(Execution::SoundComponent& component)
  {
    auto& element = component.process();
    auto handle = element.file();
    construct_ffmpeg(component);
  }

  void construct_ffmpeg(Execution::SoundComponent& component)
  {
    auto node = std::make_shared<Execution::sound_proc_type>();
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
    recompute_ffmpeg(component);
  }

  void construct_drwav(Execution::SoundComponent& component)
  {

  }


  void recompute(Execution::SoundComponent& component)
  {

  }
  void recompute_ffmpeg(Execution::SoundComponent& component)
  {
    auto& p = component.process();
    component.in_exec([n = std::dynamic_pointer_cast<ossia::nodes::sound_ref>(
                 component.OSSIAProcess().node),
             data = p.file()->m_hdl,
             upmix = p.upmixChannels(),
             start = p.startChannel(),
             startOff = p.startOffset()] {
      n->set_sound(std::move(data));
      n->set_start(start);
      n->set_start_offset(startOff);
      n->set_upmix(upmix);
    });
  }
  void recompute_drwav(Execution::SoundComponent& component)
  {

  }
};
}
namespace Execution
{
/*
class sound_mmap final : public ossia::nonowning_graph_node
{
public:
  sound_mmap()
  {
    m_outlets.push_back(&audio_out);
  }

  ~sound_mmap()
  {
  }

  void set_start(std::size_t v)
  {
    start = v;
  }
  void set_start_offset(std::size_t v)
  {
    start_offset = v;
  }
  void set_upmix(std::size_t v)
  {
    upmix = v;
  }

  void set_sound(const ossia::audio_handle& hdl)
  {
    m_handle = hdl;
    m_data.clear();
    if (hdl)
    {
      m_data.assign(m_handle->data.begin(), m_handle->data.end());
    }
  }

  void
  run(ossia::token_request t, ossia::exec_state_facade e) noexcept override
  {
    if (m_data.empty())
      return;
    const std::size_t chan = m_data.size();
    const std::size_t len = m_data[0].size();

    ossia::audio_port& ap = *audio_out.data.target<ossia::audio_port>();
    ap.samples.resize(chan);
    int64_t max_N = std::min(t.date.impl, (int64_t)(len - start_offset));
    if (max_N <= 0)
      return;
    const auto samples = max_N - t.prev_date + t.offset.impl;
    if (samples <= 0)
      return;

    if (t.date > t.prev_date)
    {
      for (std::size_t i = 0; i < chan; i++)
      {
        ap.samples[i].resize(samples);
        for (int64_t j = t.prev_date; j < max_N; j++)
        {
          ap.samples[i][j - t.prev_date + t.offset.impl]
              = m_data[i][j + start_offset];
        }
        do_fade(
            t.start_discontinuous, t.end_discontinuous, ap.samples[i],
            t.offset.impl, samples);
      }
    }
    else
    {
      // TODO rewind correctly and add rubberband
      for (std::size_t i = 0; i < chan; i++)
      {
        ap.samples[i].resize(samples);
        for (int64_t j = t.prev_date; j < max_N; j++)
        {
          ap.samples[i][max_N - (j - t.prev_date) + t.offset.impl]
              = m_data[i][j];
        }

        do_fade(
            t.start_discontinuous, t.end_discontinuous, ap.samples[i],
            max_N + t.offset.impl, t.prev_date + t.offset.impl);
      }
    }

    // Upmix
    if (upmix != 0)
    {
      if (upmix < chan)
      {
//
//    // Downmix
//    switch(upmix)
//    {
//      case 1:
//      {
//        for(std::size_t i = 1; i < chan; i++)
//        {
//          if(ap.samples[0].size() < ap.samples[i].size())
//            ap.samples[0].resize(ap.samples[i].size());
//
//          for(std::size_t j = 0; j < ap.samples[i].size(); j++)
//            ap.samples[0][j] += ap.samples[i][j];
//        }
//      }
//      default:
//        // TODO
//        break;
//    }
//
      }
      else if (upmix > chan)
      {
        switch (chan)
        {
          case 1:
          {
            ap.samples.resize(upmix);
            for (std::size_t i = 1; i < upmix; i++)
            {
              ap.samples[i] = ap.samples[0];
            }
            break;
          }
          default:
            // TODO
            break;
        }
      }
    }

    // Move channels
    if (start != 0)
    {
      ap.samples.insert(ap.samples.begin(), start, ossia::audio_channel{});
    }
  }
  std::size_t channels() const
  {
    return m_data.size();
  }
  std::size_t duration() const
  {
    return m_data.empty() ? 0 : m_data[0].size();
  }

private:
  ossia::small_vector<gsl::span<const double>, 8> m_data;
  std::size_t start{};
  std::size_t start_offset{};
  std::size_t upmix{};
  ossia::outlet audio_out{ossia::audio_port{}};
  audio_handle m_handle;
};
*/

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
  /*
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
    in_exec([n = std::dynamic_pointer_cast<ossia::nodes::sound>(
                 OSSIAProcess().node),
             data = to_double(process().file()->data()),
             upmix = process().upmixChannels(),
             start = process().startChannel(),
             startOff = process().startOffset()]() mutable {
      n->set_sound(std::move(data));
      n->set_start(start);
      n->set_start_offset(startOff);
      n->set_upmix(upmix);
    });
  }
  else*/
  {
  }
}

SoundComponent::~SoundComponent() {}
}
