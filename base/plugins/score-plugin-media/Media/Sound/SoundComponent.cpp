#include "SoundComponent.hpp"
#include <Engine/score2OSSIA.hpp>
#include <ossia/dataflow/nodes/sound.hpp>
#include <ossia/dataflow/nodes/input.hpp>
#include <ossia/dataflow/execution_state.hpp>
namespace Engine
{
namespace Execution
{

SoundComponent::SoundComponent(
    Media::Sound::ProcessModel &element,
    const Engine::Execution::Context &ctx,
    const Id<score::Component> &id,
    QObject *parent)
  : Engine::Execution::ProcessComponent_T<Media::Sound::ProcessModel, ossia::node_process>{
      element,
      ctx,
      id, "Executor::SoundComponent", parent}
{
  auto node = std::make_shared<ossia::nodes::sound>();
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);

  con(element, &Media::Sound::ProcessModel::fileChanged,
      this, [this] { this->recompute(); });
  con(element.file().decoder(), &Media::AudioDecoder::finishedDecoding,
      this, [this] { this->recompute(); });
  con(element, &Media::Sound::ProcessModel::startChannelChanged,
      this, [=] {
    in_exec(
          [node,start=process().startChannel()]
    { node->set_start(start); });
  });
  con(element, &Media::Sound::ProcessModel::upmixChannelsChanged,
      this, [=] {
    in_exec(
          [node,upmix=process().upmixChannels()
          ]
    { node->set_upmix(upmix); });
  });
  recompute();
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
  in_exec(
        [n=std::dynamic_pointer_cast<ossia::nodes::sound>(OSSIAProcess().node)
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
}

}
}



namespace Engine
{
namespace Execution
{
InputComponent::InputComponent(
    Media::Input::ProcessModel &element,
    const Engine::Execution::Context &ctx,
    const Id<score::Component> &id,
    QObject *parent)
  : Engine::Execution::ProcessComponent_T<Media::Input::ProcessModel, ossia::node_process>{
      element,
      ctx,
      id, "Executor::InputComponent", parent}
{
  auto node = std::make_shared<ossia::nodes::input>();
  this->node = node;
  m_ossia_process = std::make_shared<ossia::node_process>(node);
  con(element, &Media::Input::ProcessModel::startChannelChanged,
      this, [=] {
    in_exec(
          [node,start=process().startChannel()]
    { node->set_start(start); });
  });
  con(element, &Media::Input::ProcessModel::numChannelChanged,
      this, [=] {
    in_exec(
          [node,num=process().numChannel()]
    { node->set_num_channel(num); });
  });
  recompute();
}

void InputComponent::recompute()
{
  auto n = std::dynamic_pointer_cast<ossia::nodes::input>(OSSIAProcess().node);
  in_exec(
        [n
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
}

}
}

