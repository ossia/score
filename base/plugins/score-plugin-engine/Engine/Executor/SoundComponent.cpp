#include "SoundComponent.hpp"
#include <ossia/dataflow/audio_parameter.hpp>
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

namespace Engine
{
namespace Execution
{

SoundComponent::SoundComponent(
    Engine::Execution::IntervalComponent &parentConstraint,
    Media::Sound::ProcessModel &element,
    const Engine::Execution::Context &ctx,
    const Id<score::Component> &id,
    QObject *parent)
  : Engine::Execution::ProcessComponent_T<Media::Sound::ProcessModel, ossia::node_process>{
      parentConstraint,
      element,
      ctx,
      id, "Executor::SoundComponent", parent}
{
  auto node = std::make_shared<ossia::sound_node>();
  auto np = std::make_shared<ossia::node_process>(node);
  m_node = node;

  if(auto dest = Engine::score_to_ossia::makeDestination(ctx.devices.list(), element.outlet->address()))
    node->outputs()[0]->address = &dest->address();

  con(element, &Media::Sound::ProcessModel::fileChanged,
      this, [this] { this->recompute(); });
  con(element, &Media::Sound::ProcessModel::startChannelChanged,
      this, [this] {
    system().executionQueue.enqueue(
          [n=std::dynamic_pointer_cast<ossia::sound_node>(this->m_node)
          ,start=process().startChannel()
          ]
    { n->set_start(start); });
  });
  con(element, &Media::Sound::ProcessModel::upmixChannelsChanged,
      this, [this] {
    system().executionQueue.enqueue(
          [n=std::dynamic_pointer_cast<ossia::sound_node>(this->m_node)
          ,upmix=process().upmixChannels()
          ]
    { n->set_upmix(upmix); });
  });
  recompute();

  ctx.plugin.outlets.insert({process().outlet.get(), std::make_pair(node, node->outputs()[0])});
  ctx.plugin.execGraph->add_node(m_node);
  m_ossia_process = np;
}

void SoundComponent::recompute()
{
  auto to_double = [] (const auto& float_vec) {
    std::vector<std::vector<double>> v;
    v.reserve(float_vec.size());
    for(auto& chan : float_vec) {
      v.emplace_back(chan.begin(), chan.end());
    }
    return v;
  };
  system().executionQueue.enqueue(
        [n=std::dynamic_pointer_cast<ossia::sound_node>(this->m_node)
        ,data=to_double(process().file().data())
        ,upmix=process().upmixChannels()
        ,start=process().startChannel()
        ]
  {
    n->set_sound(std::move(data));
    n->set_start(start);
    n->set_upmix(upmix);
  });
}

SoundComponent::~SoundComponent()
{
  m_node->clear();
  system().plugin.execGraph->remove_node(m_node);
}

}
}



namespace Engine
{
namespace Execution
{

// TODO have a process that does reset it on start & stop.
// necessary for the looping to work correctly.
class OSSIA_EXPORT input_node final :
    public ossia::graph_node
{
public:
  input_node();
  ~input_node();

  void set_start(std::size_t v) { m_startChan = v; m_data.clear(); }
  void set_num_channel(std::size_t v) { m_numChan = v; m_data.clear(); }
private:
  ossia::net::node_base* get_params(ossia::execution_state& e)
  {
    auto dev = ossia::find_if(e.globalState, [] (const ossia::net::device_base* dev) {
      return dev->get_name() == "audio";
    });
    if(dev != e.globalState.end())
    {
      auto in = ossia::net::find_node((*dev)->get_root_node(), "/in/main");
      return in;
    }
    return {};
  }
  void run(ossia::token_request t, ossia::execution_state& e) override;
  std::vector<std::vector<float>> m_data;

  std::size_t m_startChan{};
  std::size_t m_numChan{};
};

input_node::input_node()
{
  m_inlets.push_back(ossia::make_inlet<ossia::audio_port>());
  m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
}

input_node::~input_node()
{

}

void input_node::run(ossia::token_request t, ossia::execution_state& e)
{
  // First read the requested channels at the end of "data".
  if(m_numChan == 0)
  {
    return;
  }
  auto in_node = get_params(e);
  if(!in_node)
    return;

  auto p = static_cast<ossia::audio_parameter*>(in_node->get_parameter());

  auto maxchan = std::min(m_startChan + m_numChan, p->audio.size());

  m_data.resize(maxchan - m_startChan);
  for(std::size_t chan = m_startChan; chan < maxchan; chan++)
  {
    const auto& src = p->audio[chan];
    auto& res = m_data[chan - m_startChan];
    std::size_t start = res.size();
    res.resize(start + (std::size_t)src.size());
    for (std::size_t i = 0; i < src.size(); i++)
      res[start + i] = src[i];
  }

  // Then copy the requested data to the output
  if(m_data.empty())
    return;

  const std::size_t chan = m_data.size();
  const auto len = (int64_t)m_data[0].size();
  ossia::audio_port& ap = *m_outlets[0]->data.target<ossia::audio_port>();
  ap.samples.resize(chan);
  int64_t max_N = std::min(t.date.impl, len);
  if(max_N <= 0)
    return;
  auto samples = max_N - m_prev_date + t.offset.impl;
  if(samples <= 0)
    return;

  if(t.date > m_prev_date)
  {
    for(std::size_t i = 0; i < chan; i++)
    {
      ap.samples[i].resize(samples);
      for(int64_t j = m_prev_date; j < max_N; j++)
      {
        ap.samples[i][j - m_prev_date + t.offset.impl] = m_data[i][j];
      }
    }
  }
  else
  {
    // TODO rewind correctly and add rubberband
    for(std::size_t i = 0; i < chan; i++)
    {
      ap.samples[i].resize(samples);
      for(int64_t j = m_prev_date; j < max_N; j++)
      {
        ap.samples[i][max_N - (j - m_prev_date) + t.offset.impl] = m_data[i][j];
      }
    }
  }
}

InputComponent::InputComponent(
    Engine::Execution::IntervalComponent &parentConstraint,
    Media::Input::ProcessModel &element,
    const Engine::Execution::Context &ctx,
    const Id<score::Component> &id,
    QObject *parent)
  : Engine::Execution::ProcessComponent_T<Media::Input::ProcessModel, ossia::node_process>{
      parentConstraint,
      element,
      ctx,
      id, "Executor::InputComponent", parent}
{
  auto node = std::make_shared<input_node>();
  auto np = std::make_shared<ossia::node_process>(node);
  m_node = node;

  if(auto dest = Engine::score_to_ossia::makeDestination(ctx.devices.list(), element.outlet->address()))
    node->outputs()[0]->address = &dest->address();
  con(element, &Media::Input::ProcessModel::startChannelChanged,
      this, [this] {
    system().executionQueue.enqueue(
          [n=std::dynamic_pointer_cast<input_node>(this->m_node)
          ,start=process().startChannel()
          ]
    { n->set_start(start); });
  });
  con(element, &Media::Input::ProcessModel::numChannelChanged,
      this, [this] {
    system().executionQueue.enqueue(
          [n=std::dynamic_pointer_cast<input_node>(this->m_node)
          ,num=process().numChannel()
          ]
    { n->set_num_channel(num); });
  });
  recompute();

  ctx.plugin.outlets.insert({process().outlet.get(), std::make_pair(node, node->outputs()[0])});
  ctx.plugin.execGraph->add_node(m_node);
  m_ossia_process = np;
}

void InputComponent::recompute()
{
  system().executionQueue.enqueue(
        [n=std::dynamic_pointer_cast<input_node>(this->m_node)
        ,num=process().numChannel()
        ,start=process().startChannel()
        ]
  {
    n->set_start(start);
    n->set_num_channel(num);
  });
}

InputComponent::~InputComponent()
{
  m_node->clear();
  system().plugin.execGraph->remove_node(m_node);
}

}
}
