#include "Executor.hpp"
#include <Engine/score2OSSIA.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <ossia/editor/scenario/time_value.hpp>
namespace Engine
{
namespace Execution
{

class SCORE_PLUGIN_MEDIA_EXPORT merger_node final :
    public ossia::graph_node
{
public:
  merger_node(int count);
  ~merger_node() override;

  void run(ossia::token_request t, ossia::execution_state& e) override;
  std::string label() const override
  {
    return "Stereo Merger";
  }
};

merger_node::merger_node(int count)
{
  for(int i = 0; i < count; i++)
  {
    auto inl = ossia::make_inlet<ossia::audio_port>();
    inl->data.target<ossia::audio_port>()->samples.resize(2);
    for(auto& channel : inl->data.target<ossia::audio_port>()->samples)
    {
      channel.reserve(512);
    }
    m_inlets.push_back(std::move(inl));
  }

  m_outlets.push_back(ossia::make_outlet<ossia::audio_port>());
  m_outlets.back()->data.target<ossia::audio_port>()->samples.resize(2 * count);
  for(auto& channel : m_outlets.back()->data.target<ossia::audio_port>()->samples)
  {
    channel.reserve(512);
  }
}

merger_node::~merger_node()
{

}

void merger_node::run(ossia::token_request t, ossia::execution_state& e)
{
  auto& out = m_outlets.back()->data.target<ossia::audio_port>()->samples;
  std::size_t cur = 0;
  for(auto inl : m_inlets)
  {
    auto& in = inl->data.target<ossia::audio_port>()->samples;

    for(std::size_t i = 0; i < std::min((std::size_t)2, in.size()); i++)
    {
      assert(cur < out.size());
      out[cur] = in[i];
      cur++;
    }
  }
}

MergerComponent::MergerComponent(
    Media::Merger::Model &element,
    const Engine::Execution::Context &ctx,
    const Id<score::Component> &id,
    QObject *parent)
  : Engine::Execution::ProcessComponent_T<Media::Merger::Model, ossia::node_process>{
      element,
      ctx,
      id, "Executor::MergerComponent", parent}
{
  auto node = std::make_shared<merger_node>(element.inCount());
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  // TODO change num of ins dynamically
}

void MergerComponent::recompute()
{
}

MergerComponent::~MergerComponent()
{
}

}
}

