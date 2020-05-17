#pragma once
#include <Media/Merger/Metadata.hpp>
#include <Process/Process.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <verdigris>

namespace Media
{
namespace Merger
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Media::Merger::Model)

  W_OBJECT(Model)

public:
  explicit Model(const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  ~Model() override;

  template <typename Impl>
  explicit Model(Impl& vis, QObject* parent) : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  int inCount() const;

public:
  void inCountChanged(int arg_1) W_SIGNAL(inCountChanged, arg_1);

public:
  void setInCount(int s);
  W_SLOT(setInCount);

private:
  int m_inCount{};

  W_PROPERTY(int, inCount READ inCount WRITE setInCount NOTIFY inCountChanged)
};
}
}
