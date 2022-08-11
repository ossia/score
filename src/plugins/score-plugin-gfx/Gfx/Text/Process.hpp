#pragma once
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <Gfx/CommandFactory.hpp>
#include <Gfx/Graph/ImageNode.hpp>
#include <Gfx/Text/Metadata.hpp>
#include <Library/LibraryInterface.hpp>

#include <score/command/PropertyCommand.hpp>
namespace Gfx::Text
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::Text::Model)
  W_OBJECT(Model)

public:
  constexpr bool hasExternalUI() { return false; }
  Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  template <typename Impl>
  Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  ~Model() override;

private:
  QString prettyName() const noexcept override;
};

using ProcessFactory = Process::ProcessFactory_T<Gfx::Text::Model>;

}
