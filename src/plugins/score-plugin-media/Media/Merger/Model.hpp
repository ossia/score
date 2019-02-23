#pragma once
#include <Media/Merger/Metadata.hpp>
#include <Process/Process.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <wobjectdefs.h>

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
  explicit Model(
      const TimeVal& duration,
      const Id<Process::ProcessModel>& id,
      QObject* parent);

  ~Model() override;

  template <typename Impl>
  explicit Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  quint64 inCount() const;

public:
  void inCountChanged(quint64 arg_1) W_SIGNAL(inCountChanged, arg_1);

public:
  void setInCount(quint64 s);
  W_SLOT(setInCount);

private:
  quint64 m_inCount{};

  W_PROPERTY(
      quint64,
      inCount READ inCount WRITE setInCount NOTIFY inCountChanged)
};
}
}
