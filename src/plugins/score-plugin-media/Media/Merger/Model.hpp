#pragma once
#include <Process/Process.hpp>

#include <Media/Merger/Metadata.hpp>

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
  enum Mode
  {
    Stereo,
    Mono
  };
  explicit Model(
      const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  ~Model() override;

  template <typename Impl>
  explicit Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
  }

  int inCount() const noexcept;
  Mode mode() const noexcept;

public:
  void inCountChanged(int arg_1) W_SIGNAL(inCountChanged, arg_1);
  void modeChanged(Mode arg_1) W_SIGNAL(modeChanged, arg_1);

public:
  void setInCount(int s);
  W_SLOT(setInCount);
  void setMode(Mode s);
  W_SLOT(setMode);

private:
  int m_inCount{};
  Mode m_mode{};

  W_PROPERTY(int, inCount READ inCount WRITE setInCount NOTIFY inCountChanged)
  W_PROPERTY(Mode, mode READ mode WRITE setMode NOTIFY modeChanged)
};
}
}

Q_DECLARE_METATYPE(Media::Merger::Model::Mode)
W_REGISTER_ARGTYPE(Media::Merger::Model::Mode)
