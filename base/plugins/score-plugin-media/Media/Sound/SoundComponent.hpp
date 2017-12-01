#pragma once
#include <Media/Sound/SoundModel.hpp>
#include <Media/Input/InputModel.hpp>
#include <Media/Effect/EffectProcessModel.hpp>
#include <Dataflow/DocumentPlugin.hpp>
#include <Engine/Executor/ProcessComponent.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <ossia/dataflow/node_process.hpp>
#include <score/model/Component.hpp>

#include <ossia/network/value/value.hpp>


namespace Engine
{
namespace Execution
{

class SoundComponent final
    : public ::Engine::Execution::
    ProcessComponent_T<Media::Sound::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("a25d0de0-74e2-4011-aeb6-4188673015f2")
public:
  SoundComponent(
      Media::Sound::ProcessModel& element,
      const ::Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void recompute();

  ~SoundComponent();

private:
  ossia::node_ptr m_node;
};

using SoundComponentFactory
= ::Engine::Execution::ProcessComponentFactory_T<SoundComponent>;



class InputComponent final
    : public ::Engine::Execution::
    ProcessComponent_T<Media::Input::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("c2ab6fe0-466a-4a33-b29a-42edd78b2a60")
public:
  InputComponent(
      Media::Input::ProcessModel& element,
      const ::Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void recompute();

  ~InputComponent();

private:
  ossia::node_ptr m_node;
};

using InputComponentFactory
= ::Engine::Execution::ProcessComponentFactory_T<InputComponent>;


class SCORE_PLUGIN_ENGINE_EXPORT EffectComponent final
    : public ::Engine::Execution::
    ProcessComponent_T<Media::Effect::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("d638adb3-64da-4b6e-b84d-7c32684fa79d")
public:
  EffectComponent(
      Media::Effect::ProcessModel& element,
      const ::Engine::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void recompute();

  ~EffectComponent();

private:
  ossia::node_ptr m_node;
};

using EffectComponentFactory
= ::Engine::Execution::ProcessComponentFactory_T<EffectComponent>;
}
}

SCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    Engine::Execution::SoundComponentFactory)

SCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    Engine::Execution::InputComponentFactory)

SCORE_CONCRETE_COMPONENT_FACTORY(
    Engine::Execution::ProcessComponentFactory,
    Engine::Execution::EffectComponentFactory)
