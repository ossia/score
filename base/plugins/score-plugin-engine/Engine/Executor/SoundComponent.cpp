#include "SoundComponent.hpp"
#include <ossia/dataflow/audio_parameter.hpp>

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
  auto np = std::make_shared<ossia::node_process>(ctx.plugin.execGraph);
  node = std::make_shared<ossia::sound_node>();
  np->node = node;

  con(element, &Media::Sound::ProcessModel::fileChanged,
      this, [this] { this->recompute(); });
  con(element, &Media::Sound::ProcessModel::startChannelChanged,
      this, [this] {
    system().executionQueue.enqueue(
          [n=std::dynamic_pointer_cast<ossia::sound_node>(this->node)
          ,start=process().startChannel()
          ]
    { n->set_start(start); });
  });
  con(element, &Media::Sound::ProcessModel::upmixChannelsChanged,
      this, [this] {
    system().executionQueue.enqueue(
          [n=std::dynamic_pointer_cast<ossia::sound_node>(this->node)
          ,upmix=process().upmixChannels()
          ]
    { n->set_upmix(upmix); });
  });
  recompute();

  ctx.plugin.nodes.insert({&element.outlet, node});
  ctx.plugin.execGraph->add_node(node);
  m_ossia_process = np;
}

void SoundComponent::recompute()
{
  system().executionQueue.enqueue(
        [n=std::dynamic_pointer_cast<ossia::sound_node>(this->node)
        ,data=process().file().data()
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
  node->clear();
  system().plugin.execGraph->remove_node(node);
}

}
}
