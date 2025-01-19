#pragma once
#include <Gfx/CommandFactory.hpp>
#include <Gfx/Graph/ImageNode.hpp>
#include <Threedim/ModelDisplay/Metadata.hpp>
#include <Library/LibraryInterface.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <score/command/PropertyCommand.hpp>
namespace Gfx::ModelDisplay
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::ModelDisplay::Model)
  W_OBJECT(Model)

public:
  constexpr bool hasExternalUI() { return false; }
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

using ProcessFactory = Process::ProcessFactory_T<Gfx::ModelDisplay::Model>;

}
