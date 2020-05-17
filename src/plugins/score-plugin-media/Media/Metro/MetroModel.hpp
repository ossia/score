#pragma once
#include <Media/Metro/MetroMetadata.hpp>
#include <Process/Process.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <ossia/detail/pod_vector.hpp>

#include <verdigris>

namespace Media
{
namespace Metro
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Media::Metro::Model)

  W_OBJECT(Model)

public:
  explicit Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  ~Model() override;

  template <typename Impl>
  explicit Model(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init();

  std::unique_ptr<Process::AudioOutlet> audio_outlet;
  std::unique_ptr<Process::Outlet> bang_outlet;
};
}
}
