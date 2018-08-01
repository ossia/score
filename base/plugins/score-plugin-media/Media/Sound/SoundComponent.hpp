#pragma once
#include <ossia/dataflow/node_process.hpp>
#include <ossia/network/value/value.hpp>

#include <Execution/ProcessComponent.hpp>
#include <Media/Input/InputModel.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <score/model/Component.hpp>

namespace Execution
{

class SoundComponent final
    : public ::Execution::
          ProcessComponent_T<Media::Sound::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("a25d0de0-74e2-4011-aeb6-4188673015f2")
public:
  SoundComponent(
      Media::Sound::ProcessModel& element,
      const ::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void recompute();

  ~SoundComponent() override;

private:
};

using SoundComponentFactory
    = ::Execution::ProcessComponentFactory_T<SoundComponent>;

class InputComponent final
    : public ::Execution::
          ProcessComponent_T<Media::Input::ProcessModel, ossia::node_process>
{
  COMPONENT_METADATA("c2ab6fe0-466a-4a33-b29a-42edd78b2a60")
public:
  InputComponent(
      Media::Input::ProcessModel& element,
      const ::Execution::Context& ctx,
      const Id<score::Component>& id,
      QObject* parent);

  void recompute();

  ~InputComponent() override;

private:
};

using InputComponentFactory
    = ::Execution::ProcessComponentFactory_T<InputComponent>;
}


SCORE_CONCRETE_COMPONENT_FACTORY(
    Execution::ProcessComponentFactory,
    Execution::SoundComponentFactory)

SCORE_CONCRETE_COMPONENT_FACTORY(
    Execution::ProcessComponentFactory,
    Execution::InputComponentFactory)
