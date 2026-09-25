#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Process.hpp>

#include <Gfx/Sink/Metadata.hpp>

#include <score_plugin_gfx_export.h>

namespace Gfx::Sink
{
class SCORE_PLUGIN_GFX_EXPORT Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Gfx::Sink::Model)
  W_OBJECT(Model)

public:
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

using ProcessFactory = Process::ProcessFactory_T<Gfx::Sink::Model>;
}
