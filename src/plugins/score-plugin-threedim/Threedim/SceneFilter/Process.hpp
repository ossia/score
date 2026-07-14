#pragma once
#include <Gfx/CommandFactory.hpp>
#include <Threedim/SceneFilter/Metadata.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

namespace Gfx::SceneFilter
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::SceneFilter::Model)
  W_OBJECT(Model)

public:
  Model(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  ~Model() override;

private:
  void init();
  QString prettyName() const noexcept override;
};

using ProcessFactory = Process::ProcessFactory_T<Gfx::SceneFilter::Model>;
}
