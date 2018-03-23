#pragma once
#include <Process/Process.hpp>
#include <Media/Merger/Metadata.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

namespace Media
{
namespace Merger
{
class Model final : public Process::ProcessModel
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Media::Merger::Model)

  Q_OBJECT
  Q_PROPERTY(quint64 inCount READ inCount WRITE setInCount NOTIFY inCountChanged)
  public:
    explicit Model(
               const TimeVal& duration,
               const Id<Process::ProcessModel>& id,
               QObject* parent);

  ~Model() override;

  template<typename Impl>
  explicit Model(
      Impl& vis,
      QObject* parent) :
    Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  quint64 inCount() const;

Q_SIGNALS:
  void inCountChanged(quint64);

public Q_SLOTS:
  void setInCount(quint64 s);

private:
  quint64 m_inCount{};
};
}
}
