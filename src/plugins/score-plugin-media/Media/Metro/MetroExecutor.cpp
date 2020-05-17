#include "MetroExecutor.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Library/LibrarySettings.hpp>
#include <Media/MediaFileHandle.hpp>
#include <Process/ExecutionContext.hpp>
#include <Scenario/Execution/score2OSSIA.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>
#include <ossia/detail/pod_vector.hpp>
#include <ossia/editor/scenario/time_value.hpp>

#include <QApplication>

namespace ossia::nodes
{
class audio_metronome final : public ossia::nonowning_graph_node
{
public:
  const ossia::small_vector<audio_sample*, 8>& hi_sound;
  const ossia::small_vector<audio_sample*, 8>& lo_sound;
  const int64_t hi_dur{}, lo_dur{};

  ossia::audio_outlet audio_out;
  ossia::value_outlet bang_out;

  struct played_sound
  {
    const ossia::small_vector<audio_sample*, 8>* samples{};
    int64_t pos{};
    int64_t dur{};
    int64_t start_sample{};
    int64_t fade_total{};
    int64_t fade_remaining{};
  };

  ossia::small_vector<played_sound, 8> in_flight;

  audio_metronome(
      const ossia::small_vector<audio_sample*, 8>& hi,
      const ossia::small_vector<audio_sample*, 8>& lo,
      int64_t hi_dur,
      int64_t lo_dur)
      : hi_sound{hi}, lo_sound{lo}, hi_dur{hi_dur}, lo_dur{lo_dur}
  {
    m_outlets.push_back(&audio_out);
    m_outlets.push_back(&bang_out);
  }

  void run(const token_request& tk, exec_state_facade st) noexcept override
  {
    if (tk.forward())
    {
      tk.metronome(
          st.modelToSamples(),
          [&](int64_t hi_start_sample) {
            for (auto& sound : in_flight)
            {
              sound.fade_total = std::min((int64_t)500, sound.dur - sound.pos);
              sound.fade_remaining = sound.fade_total;
            }

            if (hi_dur > 0)
            {
              bang_out.target<value_port>()->write_value(ossia::impulse{}, hi_start_sample);
              in_flight.push_back({&hi_sound, 0, hi_dur, hi_start_sample, 0, 0});
            }
          },
          [&](int64_t lo_start_sample) {
            for (auto& sound : in_flight)
            {
              sound.fade_total = std::min((int64_t)500, sound.dur - sound.pos);
              sound.fade_remaining = sound.fade_total;
            }

            if (lo_dur > 0)
            {
              bang_out.target<value_port>()->write_value(ossia::impulse{}, lo_start_sample);
              in_flight.push_back({&lo_sound, 0, lo_dur, lo_start_sample, 0, 0});
            }
          });
    }

    auto& ap = audio_out.target<audio_port>()->samples;

    auto render_sound = [&ap, &tk, &st](
                            const int64_t fade_total,
                            int64_t& fade_remaining,
                            int64_t& pos,
                            const int64_t dur,
                            const int64_t start_sample,
                            const ossia::small_vector<audio_sample*, 8>& sound) {
      bool finished = false;

      ap.resize(2);
      ap[0].resize(st.bufferSize());
      ap[1].resize(st.bufferSize());

      const float* const src = sound[0] + pos;

      const auto tick_start = st.physical_start(tk);

      int64_t count = st.bufferSize() - tick_start - start_sample;
      if (pos + count < dur)
      {
        pos += count;
      }
      else
      {
        count = dur - pos;
        finished = true;
      }

      auto fade_remaining_prev = fade_remaining;
      for (auto dst : {ap[0].data(), ap[1].data()})
      {
        fade_remaining = fade_remaining_prev;
        double* start = dst + tick_start + start_sample;
        for (int i = 0; i < count; i++)
        {
          const double fade = (fade_total == 0 ? 1. : double(--fade_remaining) / fade_total);

          start[i] += src[i] * fade;
        }
      }

      finished |= fade_total > 0 && fade_remaining <= 0;

      return finished;
    };

    for (auto it = in_flight.begin(); it != in_flight.end();)
    {
      bool finished = render_sound(
          it->fade_total, it->fade_remaining, it->pos, it->dur, it->start_sample, *it->samples);

      if (it->start_sample != 0)
        it->start_sample = 0;

      if (finished)
      {
        it = in_flight.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }
};
}

namespace Execution
{
struct MetronomeSounds
{
  const QString root
      = score::AppContext().settings<Library::Settings::Model>().getPath() + "/Util/";
  const std::unique_ptr<Media::AudioFile> tick{[this] {
    auto f = std::make_unique<Media::AudioFile>();
    f->load(root + "/metro_tick.wav", root + "/metro_tick.wav", Media::DecodingMethod::Libav);

    while (f->samples() != f->decodedSamples())
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      qApp->processEvents();
    }
    return f;
  }()};
  const std::unique_ptr<Media::AudioFile> tock{[this] {
    auto f = std::make_unique<Media::AudioFile>();
    f->load(root + "/metro_tock.wav", root + "/metro_tock.wav", Media::DecodingMethod::Libav);

    while (f->samples() != f->decodedSamples())
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      qApp->processEvents();
    }
    return f;
  }()};

  const Media::AudioFile::ViewHandle tick_handle{tick->handle()};
  const Media::AudioFile::ViewHandle tock_handle{tock->handle()};

  operator bool() const noexcept
  {
    return tick_handle.target<Media::AudioFile::LibavView>()
           && tock_handle.target<Media::AudioFile::LibavView>();
  }
};

MetroComponent::MetroComponent(
    Media::Metro::Model& element,
    const Execution::Context& ctx,
    const Id<score::Component>& id,
    QObject* parent)
    : Execution::ProcessComponent_T<Media::Metro::Model, ossia::node_process>{
        element,
        ctx,
        id,
        "Executor::MetroComponent",
        parent}
{
  static const MetronomeSounds sounds;
  if (sounds)
  {
    const auto& tick_sound{sounds.tick_handle.target<Media::AudioFile::LibavView>()->data};
    const auto& tock_sound{sounds.tock_handle.target<Media::AudioFile::LibavView>()->data};

    auto node = std::make_shared<ossia::nodes::audio_metronome>(
        tick_sound, tock_sound, sounds.tick->decodedSamples(), sounds.tock->decodedSamples());

    this->node = node;
    m_ossia_process = std::make_shared<ossia::node_process>(node);
  }
}

void MetroComponent::recompute() { }

MetroComponent::~MetroComponent() { }
}
