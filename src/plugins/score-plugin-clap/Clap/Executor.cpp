#include "Executor.hpp"

#include <Process/Dataflow/Port.hpp>
#include <Process/ExecutionContext.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/dataflow/execution_state.hpp>
#include <ossia/dataflow/graph_node.hpp>
#include <ossia/dataflow/port.hpp>

#include <QDebug>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Clap
{

class clap_node : public ossia::graph_node
{
public:
  clap_node(const Clap::Model& proc, Clap::PluginHandle& handle)
      : m_instance{handle}
  {
    // Create audio ports based on the model
    for(auto inlet : proc.inlets())
    {
      if(auto audio_in = qobject_cast<Process::AudioInlet*>(inlet))
      {
        m_inlets.push_back(new ossia::audio_inlet);
      }
      else if(auto ctrl_in = qobject_cast<Process::ControlInlet*>(inlet))
      {
        m_inlets.push_back(new ossia::value_inlet);
      }
    }

    for(auto outlet : proc.outlets())
    {
      if(auto audio_out = qobject_cast<Process::AudioOutlet*>(outlet))
      {
        m_outlets.push_back(new ossia::audio_outlet);
      }
    }
  }
  ~clap_node()
  {
    if(m_activated)
      deactivate_plugin();
  }
  std::string label() const noexcept override { return "clap"; }

protected:
  void activate_plugin(double sample_rate, uint32_t max_buffer_size)
  {
    if(!m_instance.plugin || m_activated)
      return;

    m_sample_rate = sample_rate;
    m_buffer_size = max_buffer_size;

    if(m_instance.plugin->activate(m_instance.plugin, sample_rate, 1, max_buffer_size))
    {
      m_activated = true;
      qDebug() << m_instance.plugin->start_processing(m_instance.plugin);
    }
  }
  void deactivate_plugin()
  {
    if(!m_instance.plugin || !m_activated)
      return;

    m_instance.plugin->stop_processing(m_instance.plugin);
    m_instance.plugin->deactivate(m_instance.plugin);
    m_activated = false;
  }

  void do_process(clap_process_t& process)
  {
    // Process audio
    process.steady_time = -1;
    clap_event_transport_t transport{
                                     .header
                                     = {
                                         .size = sizeof(clap_event_transport_t),
                                         .time = 0,
                                         .space_id = 0,
                                         .type = CLAP_EVENT_TRANSPORT,
                                         .flags = 0,
                                     },
                                     .flags = 0,
                                     .song_pos_beats = 0,
                                     .song_pos_seconds = 0,
                                     .tempo = 0,
                                     .tempo_inc = 0,
                                     .loop_start_beats = 0,
                                     .loop_end_beats = 0,
                                     .loop_start_seconds = 0,
                                     .loop_end_seconds = 0,
                                     .bar_start = 0,
                                     .bar_number = 0,
                                     .tsig_num = 4,
                                     .tsig_denom = 4};

    process.transport = &transport;
    clap_input_events evs{
        .ctx = this,
        .size = +[](const clap_input_events* list) -> uint32_t { return 0; },
        .get = +[](const clap_input_events* list,
                   uint32_t index) -> const clap_event_header_t* { return nullptr; }};
    clap_output_events_t o_evs{
        .ctx = this,
        .try_push = [](const struct clap_output_events* list,
                       const clap_event_header_t* event) -> bool { return false; }};
    process.in_events = &evs;
    process.out_events = &o_evs;

    m_instance.plugin->process(m_instance.plugin, &process);
  }
  PluginHandle& m_instance;

  bool m_activated{false};
  double m_sample_rate{44100.0};
  uint32_t m_buffer_size{512};
  std::vector<clap_audio_buffer_t> input_buffers;
  std::vector<clap_audio_buffer_t> output_buffers;
};

class clap_node_32 final : public clap_node
{
  std::vector<std::vector<float>> input_channel_storage;
  std::vector<std::vector<float>> output_channel_storage;
  std::vector<std::vector<float*>> input_channel_ptrs;
  std::vector<std::vector<float*>> output_channel_ptrs;

public:
  using clap_node::clap_node;

  void run(const ossia::token_request& t, ossia::exec_state_facade e) noexcept override
  {
    if(!m_instance.plugin)
      return;

    // Activate plugin if needed
    if(!m_activated)
    {
      activate_plugin(e.sampleRate(), e.bufferSize());
      if(!m_activated)
        return;
    }

    auto [offset, samples] = e.timings(t);
    if(samples == 0)
      return;

    // Clear previous data
    input_buffers.clear();
    output_buffers.clear();
    input_channel_storage.clear();
    output_channel_storage.clear();
    input_channel_ptrs.clear();
    output_channel_ptrs.clear();

    // Setup input buffers
    for(size_t i = 0; i < m_inlets.size(); ++i)
    {
      if(auto audio_in = m_inlets[i]->target<ossia::audio_port>())
      {
        clap_audio_buffer_t buffer{};
        buffer.data32 = nullptr;
        buffer.data64 = nullptr;
        buffer.channel_count = 2; // stereo by default
        buffer.latency = 0;
        buffer.constant_mask = 0;

        audio_in->set_channels(buffer.channel_count);
        const auto& channels = audio_in->get();
        if(!channels.empty())
        {
          buffer.channel_count = std::min((uint32_t)channels.size(), 2u);

          // Allocate float storage for input conversion
          auto& storage = input_channel_storage.emplace_back();
          storage.resize(buffer.channel_count);
          
          auto& ptrs = input_channel_ptrs.emplace_back();
          ptrs.resize(buffer.channel_count);

          storage.resize(samples * buffer.channel_count + 16);
          for(uint32_t c = 0; c < buffer.channel_count; ++c)
          {
            ptrs[c] = storage.data() + c * samples;

            // Convert from double to float
            if(c < channels.size() && channels[c].size() >= samples + offset)
            {
              const double* src = channels[c].data() + offset;
              float* dst = ptrs[c];
              for(uint32_t s = 0; s < samples; ++s)
              {
                dst[s] = static_cast<float>(src[s]);
              }
            }
            else
            {
              // Zero fill if no input data
              std::fill(ptrs[c], ptrs[c] + samples, 0.0f);
            }
          }

          buffer.data32 = ptrs.data();
        }

        input_buffers.push_back(buffer);
      }
    }

    // Setup output buffers
    for(size_t i = 0; i < m_outlets.size(); ++i)
    {
      if(auto audio_out = m_outlets[i]->target<ossia::audio_port>())
      {
        clap_audio_buffer_t buffer{};
        buffer.data32 = nullptr;
        buffer.data64 = nullptr;
        buffer.channel_count = 2; // stereo by default
        buffer.latency = 0;
        buffer.constant_mask = 0;

        audio_out->set_channels(buffer.channel_count);
        auto& channels = audio_out->get();

        // Allocate float storage for output
        auto& storage = output_channel_storage.emplace_back();
        storage.resize(buffer.channel_count);
        
        auto& ptrs = output_channel_ptrs.emplace_back();
        ptrs.resize(buffer.channel_count);

        storage.clear();
        storage.resize(samples * buffer.channel_count + 16);
        for(uint32_t c = 0; c < buffer.channel_count; ++c)
        {
          channels[c].resize(e.bufferSize());
          ptrs[c] = storage.data() + c * samples;
        }

        buffer.data32 = ptrs.data();
        output_buffers.push_back(buffer);
      }
    }

    // Process audio through CLAP plugin
    clap_process_t process{};
    process.frames_count = samples;
    process.audio_inputs = input_buffers.data();
    process.audio_outputs = output_buffers.data();
    process.audio_inputs_count = input_buffers.size();
    process.audio_outputs_count = output_buffers.size();
    do_process(process);

    // Convert output from float back to double
    size_t audio_outlet_idx = 0;
    for(size_t i = 0; i < m_outlets.size(); ++i)
    {
      if(auto audio_out = m_outlets[i]->target<ossia::audio_port>())
      {
        if(audio_outlet_idx < output_channel_storage.size())
        {
          auto& channels = audio_out->get();
          const auto& storage = output_channel_storage[audio_outlet_idx];
          
          for(uint32_t c = 0; c < std::min((uint32_t)channels.size(), (uint32_t)storage.size()); ++c)
          {
            const float* src = storage.data() + c * samples;
            double* dst = channels[c].data() + offset;
            for(uint32_t s = 0; s < samples; ++s)
            {
              dst[s] = static_cast<double>(src[s]);
            }
          }
        }
        audio_outlet_idx++;
      }
    }
  }
};

class clap_node_64 final : public clap_node
{
  std::vector<std::vector<double*>> input_channel_ptrs;
  std::vector<std::vector<double*>> output_channel_ptrs;

public:
  using clap_node::clap_node;
  void run(const ossia::token_request& t, ossia::exec_state_facade e) noexcept override
  {
    if(!m_instance.plugin)
      return;

    // Activate plugin if needed
    if(!m_activated)
    {
      activate_plugin(e.sampleRate(), e.bufferSize());
      if(!m_activated)
        return;
    }

    auto [offset, samples] = e.timings(t);
    if(samples == 0)
      return;

    // Prepare audio buffers
    input_buffers.clear();
    output_buffers.clear();

    // Setup input buffers
    size_t audio_inlet_idx = 0;
    for(size_t i = 0; i < m_inlets.size(); ++i)
    {
      if(auto audio_in = m_inlets[i]->target<ossia::audio_port>())
      {
        if(audio_inlet_idx < this->input_buffers.size())
          continue;

        clap_audio_buffer_t buffer{};
        buffer.data32 = nullptr;
        buffer.data64 = nullptr;
        buffer.channel_count = 2; // stereo by default
        buffer.latency = 0;
        buffer.constant_mask = 0;

        audio_in->set_channels(buffer.channel_count);
        const auto& channels = audio_in->get();
        if(!channels.empty())
        {
          buffer.channel_count = std::min((uint32_t)channels.size(), 2u);

          auto& ptrs = input_channel_ptrs.emplace_back();
          ptrs.resize(buffer.channel_count);

          for(uint32_t c = 0; c < buffer.channel_count; ++c)
          {
            if(c < channels.size() && channels[c].size() >= samples)
            {
              ptrs[c] = const_cast<double*>(channels[c].data() + offset);
            }
            else
            {
              ptrs[c] = nullptr;
            }
          }

          buffer.data64 = ptrs.data();
        }

        this->input_buffers.push_back(buffer);
        audio_inlet_idx++;
      }
    }

    // Setup output buffers
    size_t audio_outlet_idx = 0;
    for(size_t i = 0; i < m_outlets.size(); ++i)
    {
      if(auto audio_out = m_outlets[i]->target<ossia::audio_port>()) //FIXME
      {
        if(audio_outlet_idx < this->output_buffers.size())
          continue;

        clap_audio_buffer_t buffer{};
        buffer.data32 = nullptr;
        buffer.data64 = nullptr;
        buffer.channel_count = 2; // stereo by default
        buffer.latency = 0;
        buffer.constant_mask = 0;

        audio_out->set_channels(buffer.channel_count);
        auto& channels = audio_out->get();

        auto& ptrs = output_channel_ptrs.emplace_back();
        ptrs.resize(buffer.channel_count);

        for(uint32_t c = 0; c < buffer.channel_count; ++c)
        {
          channels[c].resize(e.bufferSize());
          ptrs[c] = channels[c].data() + offset;
        }

        buffer.data64 = ptrs.data();
        this->output_buffers.push_back(buffer);
        audio_outlet_idx++;
      }
    }

    clap_process_t process{};
    process.frames_count = samples;
    process.audio_inputs = this->input_buffers.data();
    process.audio_outputs = this->output_buffers.data();
    process.audio_inputs_count = this->input_buffers.size();
    process.audio_outputs_count = this->output_buffers.size();
    do_process(process);
  }
};

Executor::Executor(Clap::Model& proc, const Execution::Context& ctx, QObject* parent)
    : Execution::ProcessComponent_T<Clap::Model, ossia::node_process>{
          proc, ctx, "ClapComponent", parent}
{
  auto h = proc.handle();
  if(!h)
    throw std::runtime_error("Plug-in unavailable");

  if(proc.supports64())
  {
    auto node = ossia::make_node<clap_node_64>(*ctx.execState, proc, *h);
    this->node = node;
    m_ossia_process = std::make_shared<ossia::node_process>(node);
  }
  else
  {
    auto node = ossia::make_node<clap_node_32>(*ctx.execState, proc, *h);
    this->node = node;
    m_ossia_process = std::make_shared<ossia::node_process>(node);
  }
}
}
