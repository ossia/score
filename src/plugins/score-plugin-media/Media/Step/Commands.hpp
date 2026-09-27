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
//! The steps of one sequence: the one shown when the command is made, even if
//! another is shown by the time it is undone.
class ChangeSteps final : public score::Command
{
  SCORE_COMMAND_DECL(Media::CommandFactoryName(), ChangeSteps, "Change steps")
public:
  ChangeSteps(const Media::Step::Model& model, const ossia::float_vector& cur)
      : ChangeSteps{model, model.currentSequence(), cur}
  {
  }

  ChangeSteps(
      const Media::Step::Model& model, int sequence, const ossia::float_vector& cur)
      : m_model{model}
      , m_sequence{sequence}
      , m_old{model.sequences().at(sequence)}
      , m_new{cur}
  {
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    m_model.find(ctx).setSequenceSteps(m_sequence, m_old);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    m_model.find(ctx).setSequenceSteps(m_sequence, m_new);
  }

  void update(const Media::Step::Model& model, ossia::float_vector&& cur)
  {
    m_new = std::move(cur);
  }

  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_model << m_sequence << m_old << m_new;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_model >> m_sequence >> m_old >> m_new;
  }

private:
  Path<Media::Step::Model> m_model;
  int m_sequence{};
  ossia::float_vector m_old, m_new;
};

//! The number of steps of the sequence shown.
class SetStepCount final : public score::Command
{
  SCORE_COMMAND_DECL(Media::CommandFactoryName(), SetStepCount, "Set step count")
public:
  SetStepCount(const Step::Model& model, std::size_t count)
      : m_model{model}
      , m_sequence{model.currentSequence()}
      , m_old{model.steps()}
      , m_new{model.steps()}
  {
    update(model, count);
  }

  void update(const Step::Model&, std::size_t count)
  {
    m_new = m_old;
    m_new.resize(std::clamp<std::size_t>(count, 1, Step::Model::maxSteps), 0.5f);
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    m_model.find(ctx).setSequenceSteps(m_sequence, m_old);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    m_model.find(ctx).setSequenceSteps(m_sequence, m_new);
  }

  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_model << m_sequence << m_old << m_new;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_model >> m_sequence >> m_old >> m_new;
  }

private:
  Path<Media::Step::Model> m_model;
  int m_sequence{};
  ossia::float_vector m_old, m_new;
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
