#pragma once
#include <Media/Commands/MediaCommandFactory.hpp>
#include <Media/Step/Model.hpp>
#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Media
{
namespace Step
{
class ChangeSteps final : public score::Command
{
     SCORE_COMMAND_DECL(Media::CommandFactoryName(), ChangeSteps, "Change steps")
  public:
    ChangeSteps(
        const Model& model,
        const std::vector<float>& cur):
      m_model{model},
      m_old{model.steps()},
      m_new{cur}
    {
    }

    void undo(const score::DocumentContext& ctx) const
    {
      m_model.find(ctx).setSteps(m_old);
    }

    void redo(const score::DocumentContext& ctx) const
    {
      m_model.find(ctx).setSteps(m_new);
    }

    void serializeImpl(DataStreamInput& s) const
    {
      s << m_model << m_old << m_new;
    }

    void deserializeImpl(DataStreamOutput& s)
    {
      s >> m_model >> m_old >> m_new;
    }


  private:
    Path<Model> m_model;
    std::vector<float> m_old, m_new;
};

}
}
