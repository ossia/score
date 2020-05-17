#pragma once
#include <Media/Commands/MediaCommandFactory.hpp>
#include <Media/Step/Model.hpp>

#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

#include <ossia/detail/pod_vector.hpp>

namespace Media
{
class ChangeSteps final : public score::Command
{
  SCORE_COMMAND_DECL(Media::CommandFactoryName(), ChangeSteps, "Change steps")
public:
  ChangeSteps(const Media::Step::Model& model, const ossia::float_vector& cur)
      : m_model{model}, m_old{model.steps()}, m_new{cur}
  {
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    m_model.find(ctx).setSteps(m_old);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    m_model.find(ctx).setSteps(m_new);
  }

  void update(const Media::Step::Model& model, ossia::float_vector&& cur)
  {
    m_new = std::move(cur);
  }

  void serializeImpl(DataStreamInput& s) const override { s << m_model << m_old << m_new; }

  void deserializeImpl(DataStreamOutput& s) override { s >> m_model >> m_old >> m_new; }

private:
  Path<Media::Step::Model> m_model;
  ossia::float_vector m_old, m_new;
};

class SetStepCount final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(Media::CommandFactoryName(), SetStepCount, "Set step count")
public:
  SetStepCount(const Step::Model& path, std::size_t newval)
      : score::PropertyCommand{std::move(path), "stepCount", QVariant::fromValue(newval)}
  {
  }
};
class SetStepDuration final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(Media::CommandFactoryName(), SetStepDuration, "Set step duration")
public:
  SetStepDuration(const Step::Model& path, std::size_t newval)
      : score::PropertyCommand{std::move(path), "stepDuration", QVariant::fromValue(newval)}
  {
  }
};

class SetMin final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(Media::CommandFactoryName(), SetMin, "Set min")
public:
  SetMin(const Step::Model& path, double newval)
      : score::PropertyCommand{std::move(path), "min", newval}
  {
  }
};

class SetMax final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(Media::CommandFactoryName(), SetMax, "Set max")
public:
  SetMax(const Step::Model& path, double newval)
      : score::PropertyCommand{std::move(path), "max", newval}
  {
  }
};
}
